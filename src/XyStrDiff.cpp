/*
 *  StringDiff.cpp
 *  xydiff
 *
 *  Created by Frankie Dintino on 2/3/09.
 *
 */

#include "xercesc/util/PlatformUtils.hpp"


#include "Tools.hpp"
#include "DeltaException.hpp"
#include "include/XyLatinStr.hpp"
#include "xercesc/util/XMLString.hpp"
#include "xercesc/util/XMLUniDefs.hpp"

#include "xercesc/dom/DOMImplementation.hpp"
#include "xercesc/dom/DOMImplementationLS.hpp"
#include "xercesc/dom/DOMImplementationRegistry.hpp"
#include "xercesc/dom/DOMException.hpp"
#include "xercesc/dom/DOMDocument.hpp"
#include "xercesc/dom/DOMElement.hpp"
#include "xercesc/dom/DOMText.hpp"
#include "xercesc/dom/DOMTreeWalker.hpp"

#include "xercesc/dom/DOMNodeList.hpp"

#include "xercesc/dom/DOMAttr.hpp"
#include "xercesc/util/XMLUniDefs.hpp"
#include "xercesc/sax/ErrorHandler.hpp"
#include "xercesc/sax/SAXException.hpp"
#include "xercesc/sax/SAXParseException.hpp"
#include "include/XID_DOMDocument.hpp"


#include <stdlib.h>
#include <string.h>
#include <sstream>

#include "include/XyStrDiff.hpp"



/*
 * XyStrDiff functions (character-by-character string diffs)
 */

XERCES_CPP_NAMESPACE_USE 

static const XMLCh gLS[] = { chLatin_L, chLatin_S, chNull };

XyStrDiff::XyStrDiff(DOMDocument *myDoc, DOMElement *elem, const XMLCh *strX, const XMLCh *strY, XMLSize_t sizeXStr, XMLSize_t sizeYStr)
{	
	doc = myDoc;
	root = elem;
	sizex = sizeXStr;
	sizey = sizeYStr;
	
	if ((strX==NULL)||(sizex==0)) return;
	if (sizex==0) sizex = XMLString::stringLen(strX);
	if ((strY==NULL)||(sizey==0)) return;
	if (sizey==0) sizey = XMLString::stringLen(strY);

  x = XMLString::replicate(strX);
  y = XMLString::replicate(strY);

	n = sizex;
	m = sizey;

	XMLSize_t malloclen = (sizeof(XMLSize_t))*(sizex+1)*(sizey+1);
	// c = LCS Length matrix
	c = (XMLSize_t*) malloc(malloclen);
	// d = Levenshtein Distance matrix
	d = (XMLSize_t*) malloc(malloclen);
	t = (XMLSize_t*) malloc(malloclen);
	
	currop = -1;
}

/*
 * Destructor
 */

XyStrDiff::~XyStrDiff(void)
{
	free(t);
	free(c);
	free(d);
  XMLString::release(&x);
  XMLString::release(&y);
}

void XyStrDiff::LevenshteinDistance()
{
	// Step 1
	XMLSize_t k, i, j, cost, distance;
	n = XMLString::stringLen(x);
	m = XMLString::stringLen(y);

	if (n != 0 && m != 0) {
		m++;
		n++;
		// Step 2
		for(k = 0; k < n; k++) {
			c[k] = 0;
			d[k] = k;
		}
		for(k = 0; k < m; k++) {
			c[k*n] = 0;
			d[k*n] = k;
		}
		
		// Step 3 and 4	
		for(i = 1; i < n; i++) {
			for(j = 1; j < m; j++) {
				// Step 5
				if (x[i-1] == y[j-1]) {
					cost = 0;
					c[j*n+i] = c[(j-1)*n + i-1] + 1;
				} else {
					cost = 1;
					c[j*n+i] = intmax(c[(j-1)*n + i], c[j*n + i-1]);
				}
				// Step 6
				XMLSize_t del = d[j*n+i-1] + 1;
				XMLSize_t ins = d[(j-1)*n+i] + 1;
				XMLSize_t sub = d[(j-1)*n+i-1] + cost;
				if (sub <= del && sub <= ins) {
					d[j*n+i] = sub;
					t[j*n+i] = STRDIFF_SUB;
				} else if (del <= ins) {
					d[j*n+i] = del;
					t[j*n+i] = STRDIFF_DEL;
				} else {
					d[j*n+i] = ins;
					t[j*n+i] = STRDIFF_INS;
				}
			}
		}
		distance = d[n*m-1];
    this->calculatePath(sizex, sizey);
    this->flushBuffers();
    

	}
}

void XyStrDiff::calculatePath(XMLSize_t i, XMLSize_t j)
{
	if (i > 0 && j > 0 && (x[i-1] == y[j-1])) {
		this->calculatePath(i-1, j-1);
		this->registerBuffer(i-1, STRDIFF_NOOP, x[i-1]);
	} else {
		if (j > 0 && (i == 0 || c[(j-1)*n+i] >= c[j*n+i-1])) {
			this->calculatePath(i, j-1);
			this->registerBuffer(i, STRDIFF_INS, y[j-1]);
		} else if (i > 0 && (j == 0 || c[(j-1)*n+i] < c[j*n+i-1])) {
			this->calculatePath(i-1, j);
			if (i == sizex && j == sizey) {
				this->registerBuffer(i, STRDIFF_DEL, x[i-1]);
			} else {
				this->registerBuffer(i-1, STRDIFF_DEL, x[i-1]);
			}
			
		}
	}
}

void XyStrDiff::registerBuffer(XMLSize_t i, int optype, XMLCh chr)
{

	xpos = i;

	if (currop == -1) {
		currop = optype;

	} 
	if (currop == STRDIFF_SUB) {
		if (optype == STRDIFF_DEL) {
      delbuf += chr;
		} else if (optype == STRDIFF_INS) {
      insbuf += chr;
		} else {
			this->flushBuffers();
			currop = optype;
		}
	}
	else if (optype == STRDIFF_DEL) {
		currop = optype;
		delbuf += chr;
	}
	else if (optype == STRDIFF_INS) {
		currop = (currop == STRDIFF_DEL) ? STRDIFF_SUB : STRDIFF_INS;
		insbuf += chr;
	}
	else if (optype == STRDIFF_NOOP) {
		this->flushBuffers();
		currop = optype;
	}
}

void XyStrDiff::flushBuffers()
{
	XMLSize_t startpos, len;
	XMLCh tempStrA[100];
	XMLCh tempStrB[100];
	if (currop == STRDIFF_NOOP) {
		return;
	} else if (currop == STRDIFF_SUB) {
    len = delbuf.length();
		startpos = xpos - len;
		
		try {
			XMLString::transcode("tr", tempStrA, 99);
			DOMElement *r = doc->createElement(tempStrA);
			XMLString::transcode("pos", tempStrA, 99);
			XMLString::transcode(itoa(startpos).c_str(), tempStrB, 99);
			r->setAttribute(tempStrA, tempStrB);
			XMLString::transcode("len", tempStrA, 99);
			XMLString::transcode(itoa(len).c_str(), tempStrB, 99);
			r->setAttribute(tempStrA, tempStrB);

			DOMText *textNode = doc->createTextNode(insbuf.c_str());

			r->appendChild((DOMNode *)textNode);
			root->appendChild((DOMNode *)r);
		}
		catch (const XMLException& toCatch) {
			std::cout << "XMLException: " << XMLString::transcode(toCatch.getMessage()) << std::endl;
		}
		catch (const DOMException& toCatch) {
			std::cout << "DOMException: " << XMLString::transcode(toCatch.getMessage()) << std::endl;
		}
		catch (...) {
			std::cout << "Unexpected Exception: " << std::endl;
		}

    delbuf.clear();
    insbuf.clear();
	} else if (currop == STRDIFF_INS) {
		startpos = xpos;
		
		try {
			XMLString::transcode("ti", tempStrA, 99);
			DOMElement *r = doc->createElement(tempStrA);

			XMLString::transcode("pos", tempStrA, 99);
			XMLString::transcode(itoa(startpos).c_str(), tempStrB, 99);
			r->setAttribute(tempStrA, tempStrB);

			DOMText *textNode = doc->createTextNode(insbuf.c_str());

			r->appendChild((DOMNode *)textNode);
			root->appendChild((DOMNode *)r);
		}
		catch (const XMLException& toCatch) {
			std::cout << "Exception message is: \n" << XMLString::transcode(toCatch.getMessage()) << std::endl;
		}
		catch (const DOMException& toCatch) {
			std::cout << "Exception message is: \n" << XMLString::transcode(toCatch.getMessage()) << std::endl;
		}
		catch (...) {
			std::cout << "Unexpected Exception" << std::endl; 
		}

    insbuf.clear();
	} else if (currop == STRDIFF_DEL) {
    len = delbuf.length();
		startpos = xpos - len;
		try {
			XMLString::transcode("td", tempStrA, 99);
			DOMElement *r = doc->createElement(tempStrA);
			XMLString::transcode("pos", tempStrA, 99);
			XMLString::transcode(itoa(startpos).c_str(), tempStrB, 99);
			r->setAttribute(tempStrA, tempStrB);
			XMLString::transcode("len", tempStrA, 99);
			XMLString::transcode(itoa(len).c_str(), tempStrB, 99);
			r->setAttribute(tempStrA, tempStrB);		
			root->appendChild((DOMNode *)r);
		}
		catch (const XMLException& toCatch) {
			std::cout << "Exception message is: \n" << XMLString::transcode(toCatch.getMessage()) << std::endl;
		}
		catch (const DOMException& toCatch) {
			std::cout << "Exception message is: \n" << XMLString::transcode(toCatch.getMessage()) << std::endl;
		}
		catch (...) {
			std::cout << "Unexpected Exception" << std::endl;
		}
    delbuf.clear();
	}
}

