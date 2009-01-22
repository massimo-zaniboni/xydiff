#include "infra/general/Log.hpp"

#include "xercesc/util/PlatformUtils.hpp"
//#include "xercesc/dom/deprecated/DOM_NamedNodeMap.hpp"
//#include "xercesc/dom/deprecated/DOMParser.hpp"


#include "xercesc/framework/MemBufInputSource.hpp"
#include "xercesc/framework/LocalFileFormatTarget.hpp"
#include "xercesc/dom/DOMImplementation.hpp"
#include "xercesc/dom/DOMImplementationLS.hpp"
#include "xercesc/dom/DOMImplementationRegistry.hpp"
// #include "xercesc/dom/DOMBuilder.hpp"
#include "xercesc/dom/DOMException.hpp"
#include "xercesc/dom/DOMDocument.hpp"
#include "xercesc/dom/DOMElement.hpp"
#include "xercesc/dom/DOMLSOutput.hpp"
#include "xercesc/dom/DOMLSSerializer.hpp"
#include "xercesc/dom/DOMNamedNodeMap.hpp"
#include "xercesc/dom/DOMNodeList.hpp"

#include "xercesc/dom/DOMLocator.hpp"
#include "xercesc/dom/DOMAttr.hpp"
#include "xercesc/util/XMLUniDefs.hpp"
#include "xercesc/sax/ErrorHandler.hpp"
#include "xercesc/sax/SAXException.hpp"
#include "xercesc/sax/SAXParseException.hpp"

#include "include/XyLatinStr.hpp"
#include "include/XID_DOMDocument.hpp"
#include "include/XID_map.hpp"

#include "DeltaException.hpp"
#include "DOMPrint.hpp"
#include "Tools.hpp"
#include <stdio.h>
#include <iostream>
#include <fstream>

XERCES_CPP_NAMESPACE_USE
using namespace std;

static const XMLCh gLS[] = { chLatin_L, chLatin_S, chNull };

//
// Opens an XML file and if possible its associated XID-mapping file
// Apply all XIDs on the Xerces Nodes and create XID_map hash_map index to help 'getNodeWithXID()'
//

bool PrintWithXID = false ;
XID_map *PrintXidMap = NULL ;

void GlobalPrintContext_t::SetModeDebugXID( XID_map &xidmap ) { PrintXidMap = &xidmap; PrintWithXID=true; }
void GlobalPrintContext_t::ReleaseContext( void )             { PrintWithXID=false; } ;

class GlobalPrintContext_t globalPrintContext ;

// Create new XID_DOMDocument

XID_DOMDocument* XID_DOMDocument::createDocument(void) {
  DOMImplementation* impl =  DOMImplementationRegistry::getDOMImplementation(gLS);
  DOMDocument* doc = impl->createDocument();
  return new XID_DOMDocument(doc, NULL, true);
}

DOMDocument * XID_DOMDocument::getDOMDocumentOwnership() {
	DOMDocument * doc = theDocument;
	theDocument = NULL;
	return doc;
}

/* Given an XML File, construct an XID_DOMDocument
 * There are 3 possibilities concerning XIDs for the Document:
 *
 * (1) The XID-file exists, it is used
 * (2) There is no XID-file, default XIDs (1-n|n+1) are assigned
 * (3) There is no XID-file, no XIDs are assigned for the moment
 *
 */

XID_map &XID_DOMDocument::getXidMap(void) {
	return *xidmap;
	}

void XID_DOMDocument::addXidMapFile(const char *xidmapFilename) {
	if (xidmapFilename==NULL) THROW_AWAY(("empty file name"));
	std::fstream XIDmap_file(xidmapFilename, std::fstream::in);
	if (!XIDmap_file) {
		printf("Warning: no .xidmap description for XML file <%s>, using defaults Xids (1-n|n+1)\n",xidmapFilename);
		char xidstring[100];
		int nodecount = addXidMap(NULL)-1;
		if (nodecount>1) sprintf(xidstring, "(1-%d|%d)", nodecount, nodecount+1 );
		else sprintf(xidstring, "(1|2)");
		std::string mapString = xidstring ;
		XIDmap_file.close();
		std::fstream writeXidmapFile(xidmapFilename, std::fstream::out);
		if (!writeXidmapFile) {
			fprintf(stderr, "Could not attach xid description to file - write access to %s denied\n",xidmapFilename);
			THROW_AWAY(("Can not write to Xidmap file"));
			}
		else {
			writeXidmapFile << mapString << endl ;
			printf("XIDmap attached to xml file (.xidmap file created)\n");
			}
		}
	else {
		std::string mapstr;
		XIDmap_file >> mapstr ;
		addXidMap(mapstr.c_str());
		}
	return;	
	}

int XID_DOMDocument::addXidMap(const char *theXidmap) {
	if (xidmap!=NULL) THROW_AWAY(("can't attach XidMap, slot not empty"));
	TRACE("getDocumentElement()");
	DOMElement* docRoot = getDocumentElement();
	if (theXidmap==NULL) {
		int nodecount = getDocumentNodeCount() ;
		char xidstring[100];
		if (nodecount>1) sprintf(xidstring, "(1-%d|%d)", nodecount, nodecount+1 );
		else sprintf(xidstring, "(1|2)");
		TRACE("define default XidMap : " <<xidstring);
		xidmap = XID_map::addReference( new XID_map( xidstring, docRoot ) );
		return (nodecount+1);
		}
	xidmap = XID_map::addReference(new XID_map(theXidmap, docRoot));
	return -1;
	}

void XID_DOMDocument::initEmptyXidmapStartingAt(XID_t firstAvailableXid) {
	if (xidmap!=NULL) THROW_AWAY(("can't attach XidMap, slot not empty"));
	xidmap = XID_map::addReference( new XID_map() );
	if (firstAvailableXid>0) xidmap->initFirstAvailableXID(firstAvailableXid);
	}

// Construct XID_DOMDocument from a DOM_Document

XID_DOMDocument::XID_DOMDocument(DOMDocument *doc, const char *xidmapStr, bool adoptDocument) : xidmap(NULL), theDocument(doc), theParser(NULL), doReleaseTheDocument(adoptDocument)
{
	if (xidmapStr) {
		(void)addXidMap(xidmapStr);
	}
}

XID_DOMDocument::XID_DOMDocument(const char* xmlfile, bool useXidMap, bool doValidation) : xidmap(NULL), theDocument(NULL), theParser(NULL), doReleaseTheDocument(false) {
	parseDOM_Document(xmlfile, doValidation);
	
	xidmap=NULL;
	if (xmlfile==NULL) THROW_AWAY(("bad argument for xmlfile"));
	std::string xidmap_filename = std::string( xmlfile )+std::string(".xidmap") ;
  
	if (!useXidMap) {
		xidmap = XID_map::addReference( new XID_map() );
		}
	else {
		addXidMapFile(xidmap_filename.c_str());
		}
		
	}

//
// Save the XID_DOMDocument by saving the document in one file, and the XIDString in another
//

void XID_DOMDocument::SaveAs(const char *xml_filename, bool saveXidMap) {
	
	// Saves XML file
	
	//LocalFileFormatTarget xmlfile(xml_filename);
	LocalFileFormatTarget *xmlfile = new LocalFileFormatTarget(xml_filename);
	if (!xmlfile) {
		cerr << "Error: could not open <" << xml_filename << "> for output" << endl ;
		return ;
	}
	
	if (saveXidMap) {
		if (xidmap==NULL) throw VersionManagerException("Runtime Error", "XID_DOMDocument::SaveAs", "Cannot save XidMap as there is no XidMap");
		
		// Saves XID mapping
		std::string xidmap_filename = xml_filename ;
		xidmap_filename += ".xidmap" ;
	
		std::ofstream xidmapfile(xidmap_filename.c_str());
		if (!xidmapfile) {
			cerr << "Error: could not open <" << xidmap_filename << "> for output" << endl ;
			return ;
			}
		
		std::string xidString = xidmap->String() ;
		std::cout << "Version: SaveAs/xidmap->String()=" << xidString << endl ;	
		xidmapfile << xidString ;
	}

	DOMImplementation *impl = DOMImplementationRegistry::getDOMImplementation(XMLString::transcode("LS"));
	DOMLSSerializer* theSerializer = ((DOMImplementationLS*)impl)->createLSSerializer();
	DOMLSOutput *theOutput = ((DOMImplementationLS*)impl)->createLSOutput();
	
	try {
          // do the serialization through DOMLSSerializer::write();
		theOutput->setByteStream(xmlfile);
		theSerializer->write((DOMDocument*)this, theOutput);
      }
      catch (const XMLException& toCatch) {
          char* message = XMLString::transcode(toCatch.getMessage());
          std::cout << "Exception message is: \n"
               << message << "\n";
          XMLString::release(&message);
      }
      catch (const DOMException& toCatch) {
          char* message = XMLString::transcode(toCatch.msg);
          std::cout << "Exception message is: \n"
               << message << "\n";
          XMLString::release(&message);
      }
      catch (...) {
          std::cout << "Unexpected Exception \n" ;
      }
	theOutput->release();
	theSerializer->release();

	}
	
//
// Create a copy of a document
//

XID_DOMDocument* XID_DOMDocument::copy(const XID_DOMDocument *doc, bool withXID) {
	std::cout << "XID_DOMDocument::copy" << std::endl;
	XID_DOMDocument* result = XID_DOMDocument::createDocument() ;
	std::cout << "create document ok" << std::endl;
	DOMNode* resultRoot = result->importNode((DOMNode*)doc->getDocumentElement(), true);
	std::cout << "node import ok" << std::endl;
	result->appendChild(resultRoot);
	std::cout << "append child ok" << std::endl;

	if (withXID) {
		if ( doc->xidmap==NULL ) throw VersionManagerException("Program Error", "XID_DOMDocument::copy()", "Option 'withXID' used but source has NULL xidmap");
		//result.xidmap = XID_map::addReference( new XID_map( resultRoot, doc.xidmap->getFirstAvailableXID() ) );
		result->xidmap = XID_map::addReference(new XID_map(doc->xidmap->String().c_str(), resultRoot));
		std::cout << "copy xidmap ok" << std::endl;
	}
	return (result) ;	
}

XID_DOMDocument::~XID_DOMDocument() {
	this->release();
}

//---------------------------------------------------------------------------
//  Initialisation of document from File
//---------------------------------------------------------------------------

/************************************************************************************************************************************
 ***                                                    ***
 ***                PARSING XML DOCUMENT                ***
 ***                                                    ***
 ************************************************************************************************************************************/

class xydeltaParseHandler : public DOMErrorHandler {
public:
  void warning(const SAXParseException& e);
  void error(const SAXParseException& e);
  void fatalError(const SAXParseException& e);
  void resetErrors() {};
  bool handleError(const DOMError& domError);
} ;

bool xydeltaParseHandler::handleError(const DOMError& domError)
{
  DOMLocator* locator = domError.getLocation();
  cerr << "\n(GF) Error at (file " << XyLatinStr(locator->getURI()).localForm()
       << ", line " << locator->getLineNumber()
       << ", char " << locator->getColumnNumber()
       << "): " << XyLatinStr(domError.getMessage()).localForm() << endl;
  throw VersionManagerException("xydeltaParseHandler", "error", "-");
}

void xydeltaParseHandler::error(const SAXParseException& e) {
	cerr << "\n(GF) Error at (file " << XyLatinStr(e.getSystemId()).localForm()
	     << ", line " << e.getLineNumber()
	     << ", char " << e.getColumnNumber()
	     << "): " << XyLatinStr(e.getMessage()).localForm() << endl;
	throw VersionManagerException("xydeltaParseHandler", "error", "-");
	}
void xydeltaParseHandler::fatalError(const SAXParseException& e) {
	cerr << "\n(GF) Fatal Error at (file " << XyLatinStr(e.getSystemId()).localForm()
	     << ", line " << e.getLineNumber()
	     << ", char " << e.getColumnNumber()
	     << "): " << XyLatinStr(e.getMessage()).localForm() << endl;
	throw VersionManagerException("xydeltaParseHandler", "fatal error", "-");
	}
void xydeltaParseHandler::warning(const SAXParseException& e) {
	cerr << "\n(GF) Warning at (file " << XyLatinStr(e.getSystemId()).localForm()
	     << ", line " << e.getLineNumber()
	     << ", char " << e.getColumnNumber()
	     << "): " << XyLatinStr(e.getMessage()).localForm() << endl;
	}
  
void XID_DOMDocument::parseDOM_Document(const char* xmlfile, bool doValidation) {
	if (theDocument || theParser) {
		cerr << "current doc is not initialized, can not parse" << endl;
		throw VersionManagerException("XML Exception", "parseDOM_Document", "current doc is not initialized");
	}
	
	// Initialize the XML4C2 system
	try {
		XMLPlatformUtils::Initialize();
	}
	catch(const XMLException& e) {
		cerr << "Error during Xerces-c Initialization.\n"
		     << "  Exception message:" << XyLatinStr(e.getMessage()).localForm() << endl;
		ERROR("Xerces::Initialize() FAILED");
		throw VersionManagerException("XML Exception", "parseDOM_Document", "Xerces-C++ Initialization failed");
	}

	//  Create our validator, then attach an error handler to the parser.
	//  The parser will call back to methods of the ErrorHandler if it
	//  discovers errors during the course of parsing the XML document.
	//  Then parse the XML file, catching any XML exceptions that might propogate out of it.
   
        // The parser owns the Validator, so we don't have to free it
  	// The parser also owns the DOMDocument
	
	static DOMImplementation *impl = DOMImplementationRegistry::getDOMImplementation(gLS);
	theParser = ((DOMImplementationLS*)impl)->createLSParser(DOMImplementationLS::MODE_SYNCHRONOUS, 0);
	//bool ignoresWhitespace = theParser->getIncludeIgnorableWhitespace();
	
	bool errorsOccured = false;
  
	try {
		// theParser->setFeature(XMLUni::fgDOMValidation, doValidation);
		//theParser->setDoValidation(doValidation);
		DOMErrorHandler * handler = new xydeltaParseHandler();
		if (theParser->getDomConfig()->canSetParameter(XMLUni::fgDOMValidate, doValidation))
			theParser->getDomConfig()->setParameter(XMLUni::fgDOMValidate, doValidation);
		if (theParser->getDomConfig()->canSetParameter(XMLUni::fgDOMElementContentWhitespace, false))
			theParser->getDomConfig()->setParameter(XMLUni::fgDOMElementContentWhitespace, false);
		theParser->getDomConfig()->setParameter(XMLUni::fgDOMErrorHandler, handler);
	//	theParser->setErrorHandler(handler);
                theDocument = theParser->parseURI(xmlfile);
	} catch (const XMLException& e) {
		cerr << "XMLException: An error occured during parsing\n   Message: "
		     << XyLatinStr(e.getMessage()).localForm() << endl;
		errorsOccured = true;
		throw VersionManagerException("XML Exception", "parseDOM_Document", "See previous exception messages for more details");
	} catch (const DOMException& toCatch) {
		char* message = XMLString::transcode(toCatch.msg);
		cout << "Exception message is: \n"
			<< message << "\n";
		XMLString::release(&message);
	}
	catch (...) {
       cout << "Unexpected Exception \n" ;
	}
}
 
//---------------------------------------------------------------------------
// get Document(Subtree)NodeCount
//---------------------------------------------------------------------------

int XID_DOMDocument::getSubtreeNodeCount(DOMNode *node) {
	int count = 0;
	if (node==NULL) {
		FATAL("unexpected NULL node");
		abort();
	}
        if(node->hasChildNodes()) {
		DOMNode* child = node->getFirstChild();
		while(child!=NULL) {
			count += getSubtreeNodeCount(child);
			child=child->getNextSibling();
			}
		}
	count++;
	return count;
	}

int XID_DOMDocument::getDocumentNodeCount() {
	DOMElement* docRoot = this->getDocumentElement() ;
	if (docRoot==NULL) {
		ERROR("document has no Root Element");
		return 0;
	}
	return getSubtreeNodeCount( docRoot );
}

bool XID_DOMDocument::isRealData(DOMNode *node) {
	switch(node->getNodeType()) {
		
/*
 * this part needs to be optimized as it uses XyLatinStr (XML transcode)
 */ 

		case DOMNode::TEXT_NODE: {
			XyLatinStr theText( node->getNodeValue() );
			unsigned int l = strlen(theText.localForm()) ;
			for(unsigned int i=0;i<l;i++) {
				if (theText.localForm()[i]>32) return true;
				}
			return false;
			}
			
		default:
			return true;
		}
	}


void Restricted::XidTagSubtree(XID_DOMDocument *doc, DOMNode* node) {
	
	// Tag Node
	
	XID_t myXid = doc->getXidMap().getXIDbyNode(node);
	if (node->getNodeType()==DOMNode::ELEMENT_NODE) {
		char xidStr[20];
		sprintf(xidStr, "%d", (int)myXid);
		DOMNamedNodeMap* attr = node->getAttributes();
		if (attr==NULL) throw VersionManagerException("Internal Error", "XidTagSubtree()", "Element node getAttributes() returns NULL");
		// If we are dealing with the root node
		//if (doc->getDocumentNode()->isEqualNode(node)) {
		if (node->isEqualNode((DOMNode*)doc->getDocumentElement())) {
			DOMAttr* attrNodeNS = doc->createAttributeNS(
				XMLString::transcode("http://www.w3.org/2000/xmlns/"),
				XMLString::transcode("xmlns:xyd"));
				attrNodeNS->setValue(XMLString::transcode("urn:schemas-xydiff:xydelta"));
				attr->setNamedItem(attrNodeNS);
		}
<<<<<<< .working
		xercesc_3_0::DOMAttr* attrNode = doc->createAttributeNS(
			xercesc_3_0::XMLString::transcode("urn:schemas-xydiff:xydelta"),
			xercesc_3_0::XMLString::transcode("xyd:XyXid"));
=======
		DOMAttr* attrNode = doc->createAttributeNS(
			XMLString::transcode("urn:schemas-xydiff:xydelta"),
			XMLString::transcode("xyd:xid"));
>>>>>>> .merge-right.r20
		attrNode->setValue(XyLatinStr(xidStr).wideForm());
		attr->setNamedItem(attrNode);
		}
	
	
	// Apply Recursively
	
	if (node->hasChildNodes()) {
          DOMNode* child = node->getFirstChild();
		while(child!=NULL) {
			XidTagSubtree(doc, child);
			child=child->getNextSibling();
			}
		}
	}

const XMLCh * XID_DOMDocument::getNodeName() const
{
  return theDocument->getNodeName();
}

const XMLCh * XID_DOMDocument::getNodeValue() const
{
  return theDocument->getNodeValue();
}

const XMLCh * XID_DOMDocument::getXmlVersion() const
{
	return theDocument->getXmlVersion();
}

void XID_DOMDocument::setXmlVersion(const XMLCh *version)
{
	theDocument->setXmlVersion(version);
}

DOMConfiguration * XID_DOMDocument::getDOMConfig() const
{
	return theDocument->getDOMConfig();
}

DOMElement * XID_DOMDocument::createElementNS(const XMLCh *element, const XMLCh *xmlNameSpace, XMLFileLoc fileLoc1, XMLFileLoc fileLoc2)
{
	return theDocument->createElementNS(element, xmlNameSpace, fileLoc1, fileLoc2);
}

DOMXPathResult * XID_DOMDocument::evaluate(const XMLCh *xpath, const DOMNode *node, const DOMXPathNSResolver *resolver, DOMXPathResult::ResultType resultType, DOMXPathResult *result)
{
	return theDocument->evaluate(xpath, node, resolver, resultType, result);
}

bool XID_DOMDocument::getXmlStandalone() const
{
	return theDocument->getXmlStandalone();
}

void XID_DOMDocument::setXmlStandalone(bool isStandalone)
{
	theDocument->setXmlStandalone(isStandalone);
}

DOMXPathNSResolver * XID_DOMDocument::createNSResolver(const DOMNode *node)
{
	return theDocument->createNSResolver(node);
}
DOMNode::NodeType XID_DOMDocument::getNodeType() const
{
  return theDocument->getNodeType();
}

DOMNode * XID_DOMDocument::getParentNode() const
{
  return theDocument->getParentNode();
}

DOMNodeList * XID_DOMDocument::getChildNodes() const
{
  return theDocument->getChildNodes();
}

DOMNode * XID_DOMDocument::getFirstChild() const
{
  return theDocument->getFirstChild();
}

DOMNode * XID_DOMDocument::getLastChild() const
{
  return theDocument->getLastChild();
}

DOMNode * XID_DOMDocument::getPreviousSibling() const
{
  return theDocument->getPreviousSibling();
}

DOMNode * XID_DOMDocument::getNextSibling() const
{
  return theDocument->getNextSibling();
}

DOMNamedNodeMap * XID_DOMDocument::getAttributes() const
{
  return theDocument->getAttributes();
}

DOMDocument * XID_DOMDocument::getOwnerDocument() const
{
  return theDocument->getOwnerDocument();
}

DOMNode * XID_DOMDocument::cloneNode(bool deep) const
{
  return theDocument->cloneNode(deep);
}

DOMNode * XID_DOMDocument::insertBefore(DOMNode * node1, DOMNode * node2)
{
  return theDocument->insertBefore(node1, node2);
}

DOMNode * XID_DOMDocument::replaceChild(DOMNode * node1, DOMNode *node2)
{
  return theDocument->replaceChild(node1, node2);
}

DOMNode * XID_DOMDocument::removeChild(DOMNode *node1)
{
  return theDocument->removeChild(node1);
}

DOMNode * XID_DOMDocument::appendChild(DOMNode *node1)
{
  return theDocument->appendChild(node1);
}

bool XID_DOMDocument::hasChildNodes() const
{
  return theDocument->hasChildNodes();
}

void XID_DOMDocument::setNodeValue(const XMLCh *nodeValue)
{
  return theDocument->setNodeValue(nodeValue);
}

void XID_DOMDocument::normalize()
{
  return theDocument->normalize();
}

bool XID_DOMDocument::isSupported(const XMLCh *feature, const XMLCh *version) const
{
  return theDocument->isSupported(feature, version);
}

const XMLCh * XID_DOMDocument::getNamespaceURI() const
{
  return theDocument->getNamespaceURI();
}

const XMLCh * XID_DOMDocument::getPrefix() const
{
  return theDocument->getPrefix();
}

const XMLCh * XID_DOMDocument::getLocalName() const
{
  return theDocument->getLocalName();
}

void XID_DOMDocument::setPrefix(const XMLCh *prefix)
{
  return theDocument->setPrefix(prefix);
}

bool XID_DOMDocument::hasAttributes() const
{
  return theDocument->hasAttributes();
}

bool XID_DOMDocument::isSameNode(const DOMNode *node) const
{
  return theDocument->isSameNode(node);
}

bool XID_DOMDocument::isEqualNode(const DOMNode *node) const
{
  return theDocument->isEqualNode(node);
}

void * XID_DOMDocument::setUserData(const XMLCh *key, void *data, DOMUserDataHandler *handler)
{
  return theDocument->setUserData(key, data, handler);
}

void * XID_DOMDocument::getUserData(const XMLCh *key) const
{
  return theDocument->getUserData(key);
}

const XMLCh * XID_DOMDocument::getBaseURI() const
{
  return theDocument->getBaseURI();
}

// short int XID_DOMDocument::compareTreePosition(const DOMNode *node) const
// {
//   return theDocument->compareTreePosition(node);
// }

const XMLCh * XID_DOMDocument::getTextContent() const
{
  return theDocument->getTextContent();
}

void XID_DOMDocument::setTextContent(const XMLCh *textContent)
{
  return theDocument->setTextContent(textContent);
}

// const XMLCh * XID_DOMDocument::lookupNamespacePrefix(const XMLCh *namespaceURI, bool useDefault) const
// {
//   return theDocument->lookupNamespacePrefix(namespaceURI, useDefault);
// }

bool XID_DOMDocument::isDefaultNamespace(const XMLCh *namespaceURI) const
{
  return theDocument->isDefaultNamespace(namespaceURI);
}

const XMLCh * XID_DOMDocument::lookupNamespaceURI(const XMLCh *prefix) const
{
  return theDocument->lookupNamespaceURI(prefix);
}
// 
// DOMNode * XID_DOMDocument::getInterface(const XMLCh *feature)
// {
//   return theDocument->getInterface(feature);
// }

void XID_DOMDocument::release() {
	if (theParser) {
		NOTE("delete parser owning the document");
		delete theParser;
		theParser=NULL;
		theDocument=NULL;
	} else if (doReleaseTheDocument) {
		if (theDocument) {
			NOTE("delete the document");
			delete theDocument;
			theDocument=NULL;
		}
	}
	theDocument=NULL;		
	if (xidmap) {
		NOTE("delete the xidmap");
		XID_map::removeReference(xidmap);
	}
}

DOMXPathExpression * XID_DOMDocument::createExpression(const XMLCh *xpath, const DOMXPathNSResolver *resolver)
{
	return theDocument->createExpression(xpath, resolver);
}

DOMRange * XID_DOMDocument::createRange()
{
  return theDocument->createRange();
}

DOMElement * XID_DOMDocument::createElement(const XMLCh *tagName)
{
  return theDocument->createElement(tagName);
}

DOMDocumentFragment * XID_DOMDocument::createDocumentFragment()
{
  return theDocument->createDocumentFragment();
}

DOMText * XID_DOMDocument::createTextNode(const XMLCh *data)
{
  return theDocument->createTextNode(data);
}

DOMComment * XID_DOMDocument::createComment(const XMLCh *data)
{
  return theDocument->createComment(data);
}

DOMCDATASection * XID_DOMDocument::createCDATASection(const XMLCh *data)
{
  return theDocument->createCDATASection(data);
}

DOMProcessingInstruction * XID_DOMDocument::createProcessingInstruction(const XMLCh *target, const XMLCh *data)
{
  return theDocument->createProcessingInstruction(target, data);
}

DOMAttr * XID_DOMDocument::createAttribute(const XMLCh *name)
{
  return theDocument->createAttribute(name);
}

DOMEntityReference * XID_DOMDocument::createEntityReference(const XMLCh *name)
{
  return theDocument->createEntityReference(name);
}

DOMDocumentType * XID_DOMDocument::getDoctype() const
{
  return theDocument->getDoctype();
}

DOMImplementation *XID_DOMDocument::getImplementation() const
{
  return theDocument->getImplementation();
}

DOMElement * XID_DOMDocument::getDocumentElement() const
{
  return theDocument->getDocumentElement();
}

DOMNodeList * XID_DOMDocument::getElementsByTagName(const XMLCh *tagName) const
{
  return theDocument->getElementsByTagName(tagName);
}

short int XID_DOMDocument::compareDocumentPosition(const DOMNode *node) const
{
	return theDocument->compareDocumentPosition(node);
}

const XMLCh* XID_DOMDocument::lookupPrefix(const XMLCh *prefix) const
{
	return theDocument->lookupPrefix(prefix);
}

void * XID_DOMDocument::getFeature(const XMLCh *param1, const XMLCh *param2) const
{
	return theDocument->getFeature(param1, param2);
}

DOMNode * XID_DOMDocument::importNode(const DOMNode *importNode, bool deep)
{
  return theDocument->importNode(importNode, deep);
}

const XMLCh * XID_DOMDocument::getInputEncoding() const
{
	return theDocument->getInputEncoding();
}

const XMLCh * XID_DOMDocument::getXmlEncoding() const
{
	return theDocument->getXmlEncoding();
}

DOMElement * XID_DOMDocument::createElementNS(const XMLCh *namespaceURI, const XMLCh *qualifiedName)
{
  return theDocument->createElementNS(namespaceURI, qualifiedName);
}

DOMAttr * XID_DOMDocument::createAttributeNS(const XMLCh *namespaceURI, const XMLCh *qualifiedName)
{
  return theDocument->createAttributeNS(namespaceURI, qualifiedName);
}

DOMNodeList * XID_DOMDocument::getElementsByTagNameNS(const XMLCh *namespaceURI, const XMLCh *localName) const
{
  return theDocument->getElementsByTagNameNS(namespaceURI, localName);
}

DOMElement * XID_DOMDocument::getElementById(const XMLCh *elementId) const
{
  return theDocument->getElementById(elementId);
}

// const XMLCh * XID_DOMDocument::getActualEncoding() const
// {
//   return theDocument->getActualEncoding();
// }
// 
// void XID_DOMDocument::setActualEncoding(const XMLCh *actualEncoding)
// {
//   return theDocument->setActualEncoding(actualEncoding);
// }

// const XMLCh * XID_DOMDocument::getEncoding() const
// {
//   return theDocument->getEncoding();
// }
// 
// void XID_DOMDocument::setEncoding(const XMLCh *encoding)
// {
//   return theDocument->setEncoding(encoding);
// }

// bool XID_DOMDocument::getStandalone() const
// {
//   return theDocument->getStandalone();
// }

// void XID_DOMDocument::setStandalone(bool standalone)
// {
//   return theDocument->setStandalone(standalone);
// }

// const XMLCh * XID_DOMDocument::getVersion() const
// {
//   return theDocument->getVersion();
// }
// 
// void XID_DOMDocument::setVersion(const XMLCh *version)
// {
//   return theDocument->setVersion(version);
// }

const XMLCh * XID_DOMDocument::getDocumentURI() const
{
  return theDocument->getDocumentURI();
}

void XID_DOMDocument::setDocumentURI(const XMLCh *documentURI)
{
  return theDocument->setDocumentURI(documentURI);
}





bool XID_DOMDocument::getStrictErrorChecking() const
{
  return theDocument->getStrictErrorChecking();
}

void XID_DOMDocument::setStrictErrorChecking(bool strictErrorChecking)
{
  return theDocument->setStrictErrorChecking(strictErrorChecking);
}

// 
// DOMErrorHandler * XID_DOMDocument::getErrorHandler() const
// {
//   return theDocument->getErrorHandler();
// }
// 
// 
// void XID_DOMDocument::setErrorHandler(DOMErrorHandler *handler)
// {
//   return theDocument->setErrorHandler(handler);
// }


DOMNode * XID_DOMDocument::renameNode(DOMNode *n, const XMLCh *namespaceURI, const XMLCh *name)
{
  return theDocument->renameNode(n, namespaceURI, name);
}


DOMNode * XID_DOMDocument::adoptNode(DOMNode *node)
{
  return theDocument->adoptNode(node);
}


void XID_DOMDocument::normalizeDocument()
{
  return theDocument->normalizeDocument();
}


// bool XID_DOMDocument::canSetNormalizationFeature(const XMLCh *name, bool state) const
// {
//   return theDocument->canSetNormalizationFeature(name, state);
// }
// 
// 
// void XID_DOMDocument::setNormalizationFeature(const XMLCh *name, bool state)
// {
//   return theDocument->setNormalizationFeature(name, state);
// }
// 
// 
// bool XID_DOMDocument::getNormalizationFeature(const XMLCh *name) const
// {
//   return theDocument->getNormalizationFeature(name);
// }


DOMEntity * XID_DOMDocument::createEntity(const XMLCh *name)
{
  return theDocument->createEntity(name);
}


DOMDocumentType * XID_DOMDocument::createDocumentType(const XMLCh *name)
{
  return theDocument->createDocumentType(name);
}


DOMNotation * XID_DOMDocument::createNotation(const XMLCh *name)
{
  return theDocument->createNotation(name);
}


DOMElement * XID_DOMDocument::createElementNS(const XMLCh *namespaceURI, const XMLCh *qualifiedName, const XMLSSize_t lineNum, const XMLSSize_t columnNum)
{

  return theDocument->createElementNS(namespaceURI, qualifiedName, lineNum, columnNum);
}

DOMTreeWalker * XID_DOMDocument::createTreeWalker(DOMNode *root, long unsigned int whatToShow, DOMNodeFilter *filter, bool entityReferenceExpansion){
  return theDocument->createTreeWalker(root, whatToShow, filter, entityReferenceExpansion);
}


DOMNodeIterator * XID_DOMDocument::createNodeIterator(DOMNode *root, long unsigned int whatToShow, DOMNodeFilter *filter, bool entityReferenceExpansion)
{
  return theDocument->createNodeIterator(root, whatToShow, filter, entityReferenceExpansion);
}
