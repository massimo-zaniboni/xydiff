#include "xydiff/XyDelta_DOMInterface.hpp"

#include "xercesc/dom/DOMNamedNodeMap.hpp"

#include "xydiff/XID_DOMDocument.hpp"
#include "infra/general/Log.hpp"

#include "DeltaApply.hpp"
#include "DeltaReverse.hpp"


XERCES_CPP_NAMESPACE_USE

#include "xydiff/XyDiffNS.hpp"

static const XMLCh gLS[] = { chLatin_L, chLatin_S, chNull };

DOMNode* XyDelta::ReverseDelta(DOMDocument *reversedDeltaDoc, DOMNode *deltaElement) {
	try {
		DOMNode* reversed = DeltaReverse(deltaElement, reversedDeltaDoc);
		return reversed ;
	}
	catch(...) {
          return NULL;
	}
}


XID_DOMDocument* XidXyDiff(XID_DOMDocument* v0XML, const char *doc1name, XID_DOMDocument* v1XML, const char *doc2name, bool ignoreSpacesFlag=false, bool verbose=false);

// called from computeDelta()   in file VersionManager.cpp
DOMDocument* XyDelta::XyDiff(DOMDocument* doc1, const char *doc1name, 
                   DOMDocument* doc2, const char *doc2name, 
                   const char *doc1xidmap) {

	// doc1xidmap can be NULL => the default XidMap (1-n|n+1) will be used
	
	if (doc1==NULL) {
		ERRORMSG("doc1 is NULL");
		return NULL;
	}
	if (doc1name==NULL) {
		ERRORMSG("doc1name is NULL");
		return NULL;
	}
	if (doc2==NULL) {
		ERRORMSG("doc2 is NULL");
		return NULL;
	}
	if (doc2name==NULL) {
		ERRORMSG("doc2 is NULL");
		return NULL;
	}
	
	TRACE("Create XID-DOMDocument version 1");
	
	XID_DOMDocument* v0XML = new XID_DOMDocument(doc1);

	TRACE("Add XidMap to version 1");

	v0XML->addXidMap(doc1xidmap);

	TRACE("Create XID-DOMDocument version 2");

	XID_DOMDocument* v1XML = new XID_DOMDocument(doc2);
	v1XML->initEmptyXidmapStartingAt(v0XML->getXidMap().getFirstAvailableXID());
	
	TRACE("Go XidXyDiff");

	XID_DOMDocument* delta = XidXyDiff(v0XML, doc1name, v1XML, doc2name, 1, false);

        delete v0XML;
	v0XML = NULL;
        delete v1XML;
	v1XML = NULL;

	return (DOMDocument*)delta;
}

void XyDelta::SaveDomDocument(DOMDocument* d, const char *filename) {
	XID_DOMDocument* xd = new XID_DOMDocument(d);
	xd->SaveAs(filename, false);
        delete xd;
}

// DOMDocument* XyDelta::ApplyDelta(XID_DOMDocument* xdoc, DOMNode* deltaElement, bool applyAnnotations) {
// 
// 		DeltaApplyEngine appliqueDoc(xdoc);
// 		appliqueDoc.setApplyAnnotations(applyAnnotations);
// 		appliqueDoc.ApplyDeltaElement(deltaElement) ;
// 
// 		DOMImplementation* impl =  DOMImplementationRegistry::getDOMImplementation(gLS);
//         DOMDocument* resultDoc = impl->createDocument();
// 		XID_DOMDocument *resultxdoc = appliqueDoc.getResultDocument();
// 		DOMNode* resultRoot = resultDoc->importNode(resultxdoc->getDocumentElement(), true);
// 		if (applyAnnotations) {
// 			XMLCh *xyDeltaNS_ch = XMLString::transcode("urn:schemas-xydiff:unit-delta");
// 			XMLCh *xmlnsURI_ch = XMLString::transcode("http://www.w3.org/2000/xmlns/");
// 			XMLCh *xmlns_ch = XMLString::transcode("xmlns:xyu");
// 			if (!((DOMElement*)resultRoot)->hasAttributeNS(xmlnsURI_ch, xmlns_ch)) {
// 				((DOMElement*)resultRoot)->setAttributeNS(xmlnsURI_ch, xmlns_ch, xyDeltaNS_ch);
// 			}
// 			XMLString::release(&xyDeltaNS_ch);
// 			XMLString::release(&xmlnsURI_ch);
// 			XMLString::release(&xmlns_ch);
// 		}
// 
// 		return resultDoc;
// }

XID_DOMDocument* XyDelta::ApplyDelta(XID_DOMDocument* xdoc, DOMNode* deltaElement, bool applyAnnotations) {

		DeltaApplyEngine appliqueDoc(xdoc);
		appliqueDoc.setApplyAnnotations(applyAnnotations);
		appliqueDoc.ApplyDeltaElement(deltaElement) ;
	

		XID_DOMDocument *resultxdoc = appliqueDoc.getResultDocument();
		// DOMNode* resultRoot = resultxdoc->getDocumentElement();
		// if (applyAnnotations) {
		// 	XMLCh *xyDeltaNS_ch = XMLString::transcode("urn:schemas-xydiff:unit-delta");
		// 	XMLCh *xmlnsURI_ch = XMLString::transcode("http://www.w3.org/2000/xmlns/");
		// 	XMLCh *xmlns_ch = XMLString::transcode("xmlns:xyu");
		// 	if (!((DOMElement*)resultRoot)->hasAttributeNS(xmlnsURI_ch, xmlns_ch)) {
		// 		((DOMElement*)resultRoot)->setAttributeNS(xmlnsURI_ch, xmlns_ch, xyDeltaNS_ch);
		// 	}
		// 	XMLString::release(&xyDeltaNS_ch);
		// 	XMLString::release(&xmlnsURI_ch);
		// 	XMLString::release(&xmlns_ch);
		// }
		return resultxdoc;
}

unsigned int XyDelta::estimateDocumentSize(DOMDocument *doc) {
	DOMNode* root = doc->getDocumentElement();
	return (XyDelta::estimateSubtreeSize(root)+strlen("<?xml version='1.0' encoding='ISO-8859-1' ?>\n\n"));
	}

unsigned int XyDelta::estimateSubtreeSize(DOMNode *node) {
	if (node==NULL) return 0;

	unsigned int mySize = 0;

	switch(node->getNodeType()) {
		case DOMNode::ELEMENT_NODE: {
			if (node->hasChildNodes()) mySize += 2*XMLString::stringLen(node->getNodeName())+5;
			else mySize += XMLString::stringLen(node->getNodeName())+3;

			DOMNamedNodeMap* attributes = node->getAttributes();
			if (attributes!=NULL) {
				int attrCount = attributes->getLength();
				for (int i = 0; i < attrCount; i++) {
					DOMNode* attr = attributes->item(i);
					mySize += XMLString::stringLen(attr->getNodeName())+XMLString::stringLen(attr->getNodeValue())+4;
				}
			}
			
			DOMNode* child=node->getFirstChild();
			while (child!=NULL) {
				mySize += XyDelta::estimateSubtreeSize(child);
				child=child->getNextSibling();
			}
			break;
		}
		case DOMNode::TEXT_NODE: {
			mySize += XMLString::stringLen(node->getNodeValue());
			break;
		}
		default:
			return 0;
	}
	return mySize;
}

XyDOMDelta::XyDOMDelta(XID_DOMDocument* doc1p, XID_DOMDocument* doc2p, const char *doc1xidmapp)
	: doc1(doc1p), doc2(doc2p), doc1xidmap(doc1xidmapp)
{
	if (doc1==NULL) {
		ERRORMSG("doc1 is NULL");
		return;
	}
	if (doc2==NULL) {
		ERRORMSG("doc2 is NULL");
		return;
	}
	
}

XyDOMDelta::~XyDOMDelta()
{
	delete [] doc1xidmap;
}

XID_DOMDocument* XyDOMDelta::createDelta()
{
	_XyDiff_DontSaveXidmapToFile = true;
	try {
		doc1->addXidMap(doc1xidmap);
	} catch (...) {
		
	}
	//try {
		doc2->initEmptyXidmapStartingAt(doc1->getXidMap().getFirstAvailableXID());
	//} catch (...) { }
	
	XID_DOMDocument* delta = XidXyDiff(doc1, "doc1", doc2, "doc2", 1, false);
	return delta;
}