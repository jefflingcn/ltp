#include "Ltp.h"
#include <ctime>
#include <map>
#include <string>

#include "MyLib.h"
#include "Xml4nlp.h"
#include "SplitSentence.h"
#include "segment_dll.h"
#include "postag_dll.h"
#include "parser_dll.h"
#include "ner_dll.h"
#include "SRL_DLL.h"

#if _WIN32
#pragma warning(disable: 4786 4284)
#pragma comment(lib, "segmentor.lib")
#pragma comment(lib, "postagger.lib")
#pragma comment(lib, "parser.lib")
#pragma comment(lib, "ner.lib")
#pragma comment(lib, "srl.lib")
#endif

#include "codecs.hpp"
#include "logging.hpp"
#include "cfgparser.hpp"

using namespace std;

// create a platform
LTP::LTP() :
    m_ltpResource(),
    m_loaded(false) {
    ReadConfFile();
}

LTP::LTP(const char * config) :
    m_ltpResource(),
    m_loaded(false) {
    ReadConfFile(config);
}

LTP::~LTP() {
}

bool LTP::loaded() {
    return m_loaded;
}

/*
 * discard functions
 */
/*
int LTP::CreateDOMFromTxt(const char * cszTxtFileName, XML4NLP& xml) {
    return xml.CreateDOMFromFile(cszTxtFileName);
}

int LTP::CreateDOMFromXml(const char * cszXmlFileName, XML4NLP& xml) {
    return xml.LoadXMLFromFile(cszXmlFileName);
}

int LTP::SaveDOM(const char * cszSaveFileName, XML4NLP& xml) {
    return xml.SaveDOM(cszSaveFileName);
}
*/

int LTP::ReadConfFile(const char * config_file) {
    ltp::utility::ConfigParser cfg(config_file);

    if (!cfg) {
        TRACE_LOG("Failed to open config file \"%s\"", config_file);
        return -1;
    }

    std::string buffer;

    int target_mask = 0;
    // load target from config
    // initialize target mask
    if (cfg.get("target", buffer)) {
        if (buffer == "ws") {
            target_mask = (1<<1);
        } else if (buffer == "pos") {
            target_mask = ((1<<1)|(1<<2));
        } else if (buffer == "ner") {
            target_mask = ((1<<1)|(1<<2)|(1<<3));
        } else if (buffer == "dp") {
            target_mask = ((1<<1)|(1<<2)|(1<<4));
        } else if ((buffer == "srl") || (buffer == "all")) {
            target_mask = ((1<<1)|(1<<2)|(1<<3)|(1<<4)|(1<<5));
        }
    } else {
        WARNING_LOG("No \"target\" config is found, srl is set as default");
        target_mask = ((1<<1)|(1<<2)|(1<<3)|(1<<4)|(1<<5));
    }

    int loaded_mask = 0;

    if (target_mask & (1<<1)) {
        if (cfg.get("segmentor-model", buffer)) {
            // segment model item exists
            // load segmentor model
            if (0 != m_ltpResource.LoadSegmentorResource(buffer)) {
                ERROR_LOG("in LTP::wordseg, failed to load segmentor resource");
                return -1;
            }
            loaded_mask |= (1<<1);
        } else {
            WARNING_LOG("No \"segmentor-model\" config is found");
        }
    }

    if (target_mask & (1<<2)) {
        if (cfg.get("postagger-model", buffer)) {
            // postag model item exists
            // load postagger model
            if (0 != m_ltpResource.LoadPostaggerResource(buffer)) {
                ERROR_LOG("in LTP::postag, failed to load postagger resource.");
                return -1;
            }
            loaded_mask |= (1<<2);
        } else {
            WARNING_LOG("No \"postagger-model\" config is found");
        }
    }

    if (target_mask & (1<<3)) {
        if (cfg.get("ner-model", buffer)) {
            // ner model item exists
            // load ner model
            if (0 != m_ltpResource.LoadNEResource(buffer)) {
                ERROR_LOG("in LTP::ner, failed to load ner resource");
                return -1;
            }
            loaded_mask |= (1<<3);
        } else {
            WARNING_LOG("No \"ner-model\" config is found");
        }
    }

    if (target_mask & (1<<4)) {
        if (cfg.get("parser-model", buffer)) {
            //load paser model
            if ( 0 != m_ltpResource.LoadParserResource(buffer) ) {
                ERROR_LOG("in LTP::parser, failed to load parser resource");
                return -1;
            }
            loaded_mask |= (1<<4);
        } else {
            WARNING_LOG("No \"parser-model\" config is found");
        }
    }

    if (target_mask & (1<<5)) {
        if (cfg.get("srl-data", buffer)) {
            //load srl model
            if ( 0 != m_ltpResource.LoadSRLResource(buffer) ) {
                ERROR_LOG("in LTP::srl, failed to load srl resource");
                return -1;
            }
            loaded_mask |= (1<<5);
        } else {
            WARNING_LOG("No \"srl-data\" config is found");
        }
    }

    if ((loaded_mask & target_mask) != target_mask) {
        ERROR_LOG("target is config but resource not loaded.");
        return -1;
    }

    m_loaded = true;
    return 0;
}

// If you do NOT split sentence explicitly,
// this will be called according to dependencies among modules
int LTP::splitSentence_dummy(XML4NLP & xml) {
    if ( xml.QueryNote(NOTE_SENT) ) {
        return 0;
    }

    int paraNum = xml.CountParagraphInDocument();

    if (paraNum == 0) {
        ERROR_LOG("in LTP::splitsent, There is no paragraph in doc,");
        ERROR_LOG("you may have loaded a blank file or have not loaded a file yet.");
        return -1;
    }

    for (int i = 0; i < paraNum; ++i) {
        vector<string> vecSentences;
        string para;
        xml.GetParagraph(i, para);

        if (0 == SplitSentence( para, vecSentences )) {
            ERROR_LOG("in LTP::splitsent, failed to split sentence");
            return -1;
        }

        // dummy
        // vecSentences.push_back(para);
        if (0 != xml.SetSentencesToParagraph(vecSentences, i)) {
            ERROR_LOG("in LTP::splitsent, failed to write sentence to xml");
            return -1;
        }
    }

    xml.SetNote(NOTE_SENT);
    return 0;
}

// integrate word segmentor into LTP
int LTP::wordseg(XML4NLP & xml) {
    if (xml.QueryNote(NOTE_WORD)) {
        return 0;
    }

    //
    if (0 != splitSentence_dummy(xml)) {
        ERROR_LOG("in LTP::wordseg, failed to perform split sentence preprocess.");
        return -1;
    }

    /*if (0 != m_ltpResource.LoadSegmentorResource(m_ltpOption.segmentor_model_path)) {
        ERROR_LOG("in LTP::wordseg, failed to load segmentor resource");
        return -1;
    }*/

    // get the segmentor pointer
    void * segmentor = m_ltpResource.GetSegmentor();
    if (0 == segmentor) {
        ERROR_LOG("in LTP::wordseg, failed to init a segmentor");
        return -1;
    }

    int stnsNum = xml.CountSentenceInDocument();

    if (0 == stnsNum) {
        ERROR_LOG("in LTP::wordseg, number of sentence equals 0");
        return -1;
    }

    for (int i = 0; i < stnsNum; ++ i) {
        string strStn = xml.GetSentence(i);
        vector<string> vctWords;

        if (ltp::strutils::codecs::length(strStn) > MAX_SENTENCE_LEN) {
            ERROR_LOG("in LTP::wordseg, input sentence is too long");
            return -1;
        }

        if (0 == segmentor_segment(segmentor, strStn, vctWords)) {
            ERROR_LOG("in LTP::wordseg, failed to perform word segment on \"%s\"",
                    strStn.c_str());
            return -1;
        }

        if (0 != xml.SetWordsToSentence(vctWords, i)) {
            ERROR_LOG("in LTP::wordseg, failed to write segment result to xml");
            return -1;
        }
    }

    xml.SetNote(NOTE_WORD);
    return 0;
}

// integrate postagger into LTP
int LTP::postag(XML4NLP & xml) {
    if ( xml.QueryNote(NOTE_POS) ) {
        return 0;
    }

    // dependency
    if (0 != wordseg(xml)) {
        ERROR_LOG("in LTP::postag, failed to perform word segment preprocess");
        return -1;
    }

    /*if (0 != m_ltpResource.LoadPostaggerResource(m_ltpOption.postagger_model_path)) {
        ERROR_LOG("in LTP::postag, failed to load postagger resource.");
        return -1;
    }*/

    void * postagger = m_ltpResource.GetPostagger();
    if (0 == postagger) {
        ERROR_LOG("in LTP::postag, failed to init a postagger");
        return -1;
    }

    int stnsNum = xml.CountSentenceInDocument();

    if (0 == stnsNum) {
        ERROR_LOG("in LTP::postag, number of sentence equals 0");
        return -1;
    }

    for (int i = 0; i < stnsNum; ++i) {
        vector<string> vecWord;
        vector<string> vecPOS;

        xml.GetWordsFromSentence(vecWord, i);

        if (0 == vecWord.size()) {
            ERROR_LOG("Input sentence is empty.");
            return -1;
        }

        if (vecWord.size() > MAX_WORDS_NUM) {
            ERROR_LOG("Input sentence is too long.");
            return -1;
        }

        if (0 == postagger_postag(postagger, vecWord, vecPOS)) {
            ERROR_LOG("in LTP::postag, failed to perform postag on sent. #%d", i+1);
            return -1;
        }

        if (xml.SetPOSsToSentence(vecPOS, i) != 0) {
            ERROR_LOG("in LTP::postag, failed to write postag result to xml");
            return -1;
        }
    }

    xml.SetNote(NOTE_POS);

    return 0;
}

// perform ner over xml
int LTP::ner(XML4NLP &   xml) {
    if ( xml.QueryNote(NOTE_NE) ) {
        return 0;
    }

    // dependency
    if (0 != postag(xml)) {
        ERROR_LOG("in LTP::ner, failed to perform postag preprocess");
        return -1;
    }

    /*if (0 != m_ltpResource.LoadNEResource(m_ltpOption.ner_model_path)) {
        ERROR_LOG("in LTP::ner, failed to load ner resource");
        return -1;
    }*/

    void * ner = m_ltpResource.GetNER();

    if (NULL == ner) {
        ERROR_LOG("in LTP::ner, failed to init a ner.");
        return -1;
    }

    int stnsNum = xml.CountSentenceInDocument();

    if (stnsNum == 0) {
        ERROR_LOG("in LTP::ner, number of sentence equals 0");
        return -1;
    }

    for (int i = 0; i < stnsNum; ++ i) {
        vector<string> vecWord;
        vector<string> vecPOS;
        vector<string> vecNETag;

        if (xml.GetWordsFromSentence(vecWord, i) != 0) {
            ERROR_LOG("in LTP::ner, failed to get words from xml");
            return -1;
        }

        if (xml.GetPOSsFromSentence(vecPOS, i) != 0) {
            ERROR_LOG("in LTP::ner, failed to get postags from xml");
            return -1;
        }

        if (0 == vecWord.size()) {
            ERROR_LOG("Input sentence is empty.");
            return -1;
        }

        if (vecWord.size() > MAX_WORDS_NUM) {
            ERROR_LOG("Input sentence is too long.");
            return -1;
        }

        if (0 == ner_recognize(ner, vecWord, vecPOS, vecNETag)) {
            ERROR_LOG("in LTP::ner, failed to perform ner on sent. #%d", i+1);
            return -1;
        }

        xml.SetNEsToSentence(vecNETag, i);
    }

    xml.SetNote(NOTE_NE);
    return 0;
}

int LTP::parser(XML4NLP & xml) {
    if ( xml.QueryNote(NOTE_PARSER) ) return 0;

    if (0 != postag(xml)) {
        ERROR_LOG("in LTP::parser, failed to perform postag preprocessing");
        return -1;
    }

    /*if ( 0 != m_ltpResource.LoadParserResource(m_ltpOption.parser_model_path) ) {
        ERROR_LOG("in LTP::parser, failed to load parser resource");
        return -1;
    }*/

    void * parser = m_ltpResource.GetParser();

    if (parser == NULL) {
        ERROR_LOG("in LTP::parser, failed to init a parser");
        return -1;
    }

    int stnsNum = xml.CountSentenceInDocument();
    if (stnsNum == 0) {
        ERROR_LOG("in LTP::parser, number of sentences equals 0");
        return -1;
    }

    for (int i = 0; i < stnsNum; ++i) {
        vector<string>  vecWord;
        vector<string>  vecPOS;
        vector<int>     vecHead;
        vector<string>  vecRel;

        if (xml.GetWordsFromSentence(vecWord, i) != 0) {
            ERROR_LOG("in LTP::parser, failed to get words from xml");
            return -1;
        }

        if (xml.GetPOSsFromSentence(vecPOS, i) != 0) {
            ERROR_LOG("in LTP::parser, failed to get postags from xml");
            return -1;
        }

        if (0 == vecWord.size()) {
            ERROR_LOG("Input sentence is empty.");
            return -1;
        }

        if (vecWord.size() > MAX_WORDS_NUM) {
            ERROR_LOG("Input sentence is too long.");
            return -1;
        }

        if (-1 == parser_parse(parser, vecWord, vecPOS, vecHead, vecRel)) {
            ERROR_LOG("in LTP::parser, failed to perform parse on sent. #%d", i+1);
            return -1;
        }

        if (0 != xml.SetParsesToSentence(vecHead, vecRel, i)) {
            ERROR_LOG("in LTP::parser, failed to write parse result to xml");
            return -1;
        }
    }

    xml.SetNote(NOTE_PARSER);

    return 0;
}

int LTP::srl(XML4NLP & xml) {
    if ( xml.QueryNote(NOTE_SRL) ) return 0;

    // dependency
    if (0 != ner(xml)) {
        ERROR_LOG("in LTP::srl, failed to perform ner preprocess");
        return -1;
    }

    if (0 != parser(xml)) {
        ERROR_LOG("in LTP::srl, failed to perform parsing preprocess");
        return -1;
    }

    /*if ( 0 != m_ltpResource.LoadSRLResource(m_ltpOption.srl_data_dir) ) {
        ERROR_LOG("in LTP::srl, failed to load srl resource");
        return -1;
    }*/

    int stnsNum = xml.CountSentenceInDocument();
    if (stnsNum == 0) {
        ERROR_LOG("in LTP::srl, number of sentence equals 0");
        return -1;
    }

    for (int i = 0; i < stnsNum; ++i) {
        vector<string>              vecWord;
        vector<string>              vecPOS;
        vector<string>              vecNE;
        vector< pair<int, string> > vecParse;
        vector< pair< int, vector< pair<const char *, pair< int, int > > > > > vecSRLResult;

        if (xml.GetWordsFromSentence(vecWord, i) != 0) {
            ERROR_LOG("in LTP::ner, failed to get words from xml");
            return -1;
        }

        if (xml.GetPOSsFromSentence(vecPOS, i) != 0) {
            ERROR_LOG("in LTP::ner, failed to get postags from xml");
            return -1;
        }

        if (xml.GetNEsFromSentence(vecNE, i) != 0) {
            ERROR_LOG("in LTP::ner, failed to get ner result from xml");
            return -1;
        }

        if (xml.GetParsesFromSentence(vecParse, i) != 0) {
            ERROR_LOG("in LTP::ner, failed to get parsing result from xml");
            return -1;
        }

        if (0 != SRL(vecWord, vecPOS, vecNE, vecParse, vecSRLResult)) {
            ERROR_LOG("in LTP::srl, failed to perform srl on sent. #%d", i+1);
            return -1;
        }

        int j = 0;
        for (; j < vecSRLResult.size(); ++j) {
            vector<string>              vecType;
            vector< pair<int, int> >    vecBegEnd;
            int k = 0;

            for (; k < vecSRLResult[j].second.size(); ++k) {
                vecType.push_back(vecSRLResult[j].second[k].first);
                vecBegEnd.push_back(vecSRLResult[j].second[k].second);
            }

            if (0 != xml.SetPredArgToWord(i, vecSRLResult[j].first, vecType, vecBegEnd)) {
                return -1;
            }
        }
    }

    xml.SetNote(NOTE_SRL);
    return 0;
}

