#pragma once
class ISAXCallback {};
namespace ATG {
class XMLParser {
public:
    XMLParser();
    ~XMLParser();
    HRESULT ParseXMLBuffer(const CHAR* buffer, UINT size);
    void RegisterSAXCallbackInterface(ISAXCallback* pCallback);
    void DestroyParser();
};
}
