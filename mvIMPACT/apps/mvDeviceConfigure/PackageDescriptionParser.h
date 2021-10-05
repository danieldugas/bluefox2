//-----------------------------------------------------------------------------
#ifndef PackageDescriptionParserH
#define PackageDescriptionParserH PackageDescriptionParserH
//-----------------------------------------------------------------------------
#include <string.h>
#include <Toolkits/expat/include/ExpatImpl.h>
#include <map>
#include <vector>
#include <apps/Common/wxAbstraction.h>

//-----------------------------------------------------------------------------
struct SuitableProductKey
//-----------------------------------------------------------------------------
{
    wxString name_;
    Version revisionMin_;
    Version revisionMax_;
    explicit SuitableProductKey() : name_() {}
};

//-----------------------------------------------------------------------------
struct ProductUpdateURLs
//-----------------------------------------------------------------------------
{
    wxString productFamily_;
    wxString updateArchiveUrl_;
    wxString releaseNotesUrl_;
    wxString packageDescriptionUrl_;
    wxString timestamp_;
    explicit ProductUpdateURLs() : productFamily_(), updateArchiveUrl_(), releaseNotesUrl_(), packageDescriptionUrl_(), timestamp_() {}
};

//-----------------------------------------------------------------------------
struct FileEntry
//-----------------------------------------------------------------------------
{
    wxString name_;
    wxString type_;
    wxString description_;
    Version version_;
    std::map<wxString, SuitableProductKey> suitableProductKeys_;
    wxString localMVUFileName_;
    bool boFromOnline_;
    ProductUpdateURLs onlineUpdateInfos_;
    explicit FileEntry() : name_(), type_(), description_(), localMVUFileName_(), boFromOnline_( false ), onlineUpdateInfos_() {}
};

typedef std::vector<FileEntry> FileEntryContainer;
typedef std::map<std::string, FileEntry> ProductToFileEntryMap;

//-----------------------------------------------------------------------------
class PackageDescriptionFileParser : public CExpatImpl
//-----------------------------------------------------------------------------
{
    // internal types
    enum TTagType
    {
        ttInvalid,
        ttPackageDescription,
        ttFile,
        ttSuitableProductKey
    };
    static const std::string    m_ATTR_NAME;
    static const std::string    m_ATTR_TYPE;
    static const std::string    m_ATTR_DESCRIPTION;
    static const std::string    m_ATTR_VERSION;
    static const std::string    m_ATTR_REVISION_MIN;
    static const std::string    m_ATTR_REVISION_MAX;

    wxString                    m_lastError;
    FileEntryContainer          m_results;
    Version                     m_packageDescriptionVersion;

    TTagType                    GetTagType( const char* tagName ) const;
public:
    void                        OnPostCreate( void );
    void                        OnStartElement( const XML_Char* pszName, const XML_Char** papszAttrs );
    const wxString&             GetLastError( void ) const
    {
        return m_lastError;
    }
    const FileEntryContainer&   GetResults( void ) const
    {
        return m_results;
    }
};

#endif // PackageDescriptionParserH
