#include "pch.h"
#include "PointerManager.h"
#include "curl/curl.h" // to get PointerData.xml from github
#include <winver.h> // to get version string of MCC
#include <pugixml.hpp>
#include "HCMDirPath.h"
#include "HaloEnums.h"
#define useDevPointerData 1
#define debugPointerManager 1

PointerManager* PointerManager::instance = nullptr;
typedef std::tuple<std::string, std::optional<GameState>> DataKey;

enum class MCCProcessType
{
    Steam,
    WinStore
};

constexpr std::string_view xmlFileName = "PointerData.xml";
constexpr std::string_view githubPath = "https://raw.githubusercontent.com/Burnt-o/HaloCheckpointManager/HCM3/HCMInternal/PointerData.xml";

class PointerManager::PointerManagerImpl {


    private:
        std::string pointerDataLocation;

        // Functions run by constructor, in order of execution
        void downloadXML(std::string_view url);
        std::string readLocalXML();
        VersionInfo getCurrentMCCVersion();
        MCCProcessType getCurrentMCCType();
        void parseXML(std::string& xml);

        void processVersionedEntry(pugi::xml_node entry);
        void instantiateMultilevelPointer(pugi::xml_node entry, std::string entryType, DataKey dKey);
        void instantiateMidhookContextInterpreter(pugi::xml_node entry, DataKey dKey);
        template <typename T>
        void instantiateVectorFloat(pugi::xml_node entry, DataKey dKey);
        template <typename T>
        void instantiateVectorInteger(pugi::xml_node entry, DataKey dKey);
        void instantiateVectorString(pugi::xml_node entry, DataKey dKey);


        std::string currentGameVersion;
        MCCProcessType currentProcessType;


    public:
        PointerManagerImpl();
        ~PointerManagerImpl() = default;

        // data mapped by strings
        static std::map<DataKey,std::any> mAllData;
};

std::map<DataKey, std::any> PointerManager::PointerManagerImpl::mAllData{};



PointerManager::PointerManager() : impl(new PointerManagerImpl) 
{
    if (instance != nullptr)
    {
        throw HCMInitException("Cannot have more than one PointerManager");
    }
    instance = this;
}
PointerManager::~PointerManager() = default; // https://www.fluentcpp.com/2017/09/22/make-pimpl-using-unique_ptr/

PointerManager::PointerManagerImpl::PointerManagerImpl()
{

#if debugPointerManager == 0
    // set plog severity to info temporarily
    auto oldSeverity = plog::get()->getMaxSeverity();
    plog::get()->setMaxSeverity(plog::info);
#endif

    // Set pointerDataLocation 
    pointerDataLocation = HCMDirPath::GetHCMDirPath();
    pointerDataLocation += xmlFileName;

    try
    {
        PLOG_INFO << "downloading pointerData.xml";
        downloadXML(githubPath);
    }
    catch (HCMInitException ex)
    {
        PLOG_ERROR << "Failed to download HCM PointerData xml, trying local backup";
    }


    std::string pointerData = readLocalXML();
    std::stringstream buf; buf << getCurrentMCCVersion();
    this->currentGameVersion = buf.str();
    this->currentProcessType = getCurrentMCCType();
    PLOG_INFO << "MCC Version: " << currentGameVersion;

    parseXML(pointerData);
    PLOG_DEBUG << "pointer parsing complete";
    //for (const auto& [key, value] : mMultilevelPointerData)
    //{
    //    PLOG_DEBUG << std::format("stored key: {}", key);
    //}


#if debugPointerManager == 0
    // set severity back to normal
    plog::get()->setMaxSeverity(oldSeverity);
#endif

}

template <typename T>
T PointerManager::getData(std::string dataName, std::optional<GameState> game)
{

    auto key = DataKey(dataName, game);
    // Check data exists
    if (!instance->impl.get()->mAllData.contains(key))
    {
        PLOG_ERROR << "no valid pointer data for " << dataName;
        throw HCMInitException(std::format("pointerData was null for {}", dataName));
    }

    // Check correct type
    auto& type = instance->impl.get()->mAllData.at(key).type();
    if (!(typeid(T) == type))
    {
        throw HCMInitException(std::format("Invalid type access for {}\nType was {} but {} was requested", dataName, type.name(), typeid(T).name()));
    }
    
    //    return std::any_cast<T>(instance->impl.get()->mAllData.at(key));
    return std::any_cast<T>(instance->impl.get()->mAllData.at(key));
}

// explicit template instantiations of PointerManager::getData
template
std::shared_ptr<MultilevelPointer> PointerManager::getData(std::string dataName, std::optional<GameState> game);



std::string PointerManager::PointerManagerImpl::readLocalXML()
{
    std::string pathToFile;
#if HCM_DEBUG
    pathToFile = "C:\\IMP\\CODE\\HaloCheckpointManager\\HCMInternal\\PointerData.xml";
#else
    pathToFile = pointerDataLocation;
#endif

    std::ifstream inFile(pathToFile.c_str());
    if (inFile.is_open())
    {
        std::stringstream buffer;
        buffer << inFile.rdbuf();
        inFile.close();
        PLOG_INFO << "local XML read succesfully!";
        return buffer.str();
    }
    else
    {
        std::stringstream er;
        er << "Failed to open file : " << GetLastError() << std::endl << "at: " << pathToFile;
        throw HCMInitException(er.str().c_str());
    }

}


#pragma region libcurl helpers
// write the data into a `std::string` rather than to a file.
std::size_t write_data(void* buf, std::size_t size, std::size_t nmemb,
    void* userp)
{
    if (auto sp = static_cast<std::string*>(userp))
    {
        sp->append(static_cast<char*>(buf), size * nmemb);
        return size * nmemb;
    }

    return 0;
}

// A deleter to use in the smart pointer for automatic cleanup
struct curl_dter {
    void operator()(CURL* curl) const
    {
        if (curl) curl_easy_cleanup(curl);
    }
};

// A smart pointer to automatically clean up out CURL session
using curl_uptr = std::unique_ptr<CURL, curl_dter>;
#pragma endregion //libcurl helpers

void PointerManager::PointerManagerImpl::downloadXML(std::string_view url)
{
    // Download from the intertubes
    std::string xmlString;
    if (auto curl = curl_uptr(curl_easy_init()))
    {
        curl_easy_setopt(curl.get(), CURLOPT_URL, url);
        curl_easy_setopt(curl.get(), CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &xmlString);

        CURLcode ec;
        if ((ec = curl_easy_perform(curl.get())) != CURLE_OK)
            throw HCMInitException(curl_easy_strerror(ec));

    }

    // Write to local file
    std::string pathToFile = pointerDataLocation;
    std::ofstream outFile(pathToFile.c_str());
    if (outFile.is_open())
    {
        outFile << xmlString;
        outFile.close();
        PLOG_INFO << "local XML file written!";
        return;
    }
    else
    {
        std::stringstream er;
        er << "Failed to write file : " << GetLastError() << std::endl << "at: " << pathToFile;
        throw HCMInitException(er.str().c_str());
    }

}




VersionInfo PointerManager::PointerManagerImpl::getCurrentMCCVersion()
{
    VersionInfo outCurrentMCCVersion;
    HMODULE mccProcess = GetModuleHandle(NULL);
    char mccProcessPath[MAX_PATH];
    GetModuleFileNameA(mccProcess, mccProcessPath, sizeof(mccProcessPath));

    PLOG_DEBUG << "Getting file version info of mcc at: " << mccProcessPath;
    outCurrentMCCVersion = getFileVersion(mccProcessPath);

    PLOG_DEBUG << "mccVersionInfo: " << outCurrentMCCVersion;

    if (outCurrentMCCVersion.major != 1)
    {
        std::stringstream buf;
        buf << outCurrentMCCVersion;
        throw HCMInitException(std::format("mccVersionInfo did not start with \"1.\"! Actual read version: {}", buf.str()).c_str());
    }

    return outCurrentMCCVersion;

}


MCCProcessType PointerManager::PointerManagerImpl::getCurrentMCCType()
{
    std::string outCurrentMCCType;
    HMODULE mccProcess = GetModuleHandle(NULL);
    char mccProcessPath[MAX_PATH];
    GetModuleFileNameA(mccProcess, mccProcessPath, sizeof(mccProcessPath));

    std::string mccName = mccProcessPath;
    mccName = mccName.substr(mccName.find_last_of("\\") + 1, mccName.size() - mccName.find_last_of("\\") - 1);

    // checks need to ignore letter case
    if (boost::iequals(mccName, "MCCWinStore-Win64-Shipping.exe")) 
    {
        PLOG_DEBUG << "setting process type to WinStore";
        return MCCProcessType::WinStore;
    }
    else if (boost::iequals(mccName, "MCC-Win64-Shipping.exe"))
    {
        PLOG_DEBUG << "setting process type to Steam";
        return MCCProcessType::Steam;
    }
    else
    {
        throw HCMInitException(std::format("MCC process had the wrong name!: {}", mccName));
    }

}



void PointerManager::PointerManagerImpl::parseXML(std::string& xml)
{
    using namespace pugi;
    xml_document doc;
    xml_parse_result result = doc.load_string(xml.c_str());
    if (!result)
    {
        std::stringstream er;
        er << "XML [" << xml << "] parsed with errors, attr value: [" << doc.child("node").attribute("attr").value() << "]\n";
        er << "Error description: " << result.description() << "\n";
        er << "Error offset: " << result.offset << " (error at [..." << (xml.c_str() + result.offset) << "]\n\n";
        er << "xml in question: \n\n" << xml;
        throw HCMInitException(er.str());
    }

    xml_node root = doc.child("root");

    for (xml_node entry = root.first_child(); entry; entry = entry.next_sibling())
    //for (xml_node entry : root.children())
    {
        std::string entryName = entry.name();
        PLOG_DEBUG << "Processing entry, name: " << entryName;
        if (entryName == "VersionedEntry")
        {
                processVersionedEntry(entry);
        }
        else
        {
            PLOG_ERROR << "Unexpected item in pointer data: " << entry.name();
        }

    }



}


void PointerManager::PointerManagerImpl::processVersionedEntry(pugi::xml_node entry)
{
    using namespace pugi;
    std::string entryName = entry.attribute("Name").value();
    std::optional<GameState> entryGame = std::nullopt;

    PLOG_DEBUG << "processing versionedEntry, name: " << entry.name();
    for (xml_node versionEntry = entry.first_child(); versionEntry; versionEntry = versionEntry.next_sibling())
    {


        if (strcmp(versionEntry.attribute("Version").value(), currentGameVersion.c_str()) != 0 &&(strcmp(versionEntry.attribute("Version").value(), "All") != 0)) // We only want the versionEntries of the current MCC version
        {
                PLOG_VERBOSE << "No version match";
                continue;
        }
        PLOG_DEBUG << "Matching version found";

        // check for steam vs winstore flag
        if (!versionEntry.attribute("ProcessType").empty())
        {
            std::string processTypeString = versionEntry.attribute("ProcessType").as_string();
            constexpr std::string_view steamString = "Steam";
            constexpr std::string_view winString = "WinStore";
            PLOG_DEBUG << "processTypeString.length()" << processTypeString.size();



            PLOG_DEBUG << "Checking process type";
            if (
                (currentProcessType == MCCProcessType::Steam && strcmp(processTypeString.c_str(), steamString.data()) != 0)
                || (currentProcessType == MCCProcessType::WinStore && strcmp(processTypeString.c_str(), winString.data()) != 0)
                )
            {
                PLOG_VERBOSE << "currentProcessType: " << (int)currentProcessType;
                PLOG_DEBUG << "wrong process type, value read: " << versionEntry.attribute("ProcessType").value();
                continue;
            }
        }

        // check for game specificity
        if (!versionEntry.attribute("Game").empty())
        {
            std::string gameString = versionEntry.attribute("Game").value();
            entryGame = StringToGameState.at(gameString);
        }

        // Construct dataKey
        auto dKey = DataKey(entryName, entryGame);

                // check for duplicates
        if (mAllData.contains(dKey))
        {
            throw HCMInitException(std::format("Entry already exists in pointerData, entryName {}\n", entryName));
        }

        std::string entryType = entry.attribute("Type").value(); // Convert to std::string
        // Check what type it is
        if (entryType.starts_with("MultilevelPointer"))
        {
            PLOG_DEBUG << "instantiating MultilevelPointer";
            instantiateMultilevelPointer(versionEntry, entryType, dKey);

        }
        else if (entryType.starts_with("MidhookContextInterpreter"))
        {
            PLOG_DEBUG << "instantiating MidhookContextInterpreter";
            instantiateMidhookContextInterpreter(versionEntry, dKey);
        }
        else if (entryType.starts_with("Vector"))
        {
            PLOG_DEBUG << "instantiating Vector";
            std::string typeName = entry.attribute("Typename").as_string();
            // I'll have to manually add each type I want to support here.
            if (typeName == nameof(byte))
                instantiateVectorInteger<byte>(versionEntry, dKey);
            else if (typeName == nameof(int))
                instantiateVectorInteger<int>(versionEntry, dKey);
            else if (typeName == nameof(long))
                instantiateVectorInteger<long>(versionEntry, dKey);
            else if (typeName == nameof(float))
                instantiateVectorFloat<float>(versionEntry, dKey);
            else if (typeName == nameof(double))
                instantiateVectorFloat<double>(versionEntry, dKey);
            else if (typeName == nameof(long double))
                instantiateVectorFloat<long double>(versionEntry, dKey);
            else if (typeName == nameof(std::string))
                instantiateVectorString(versionEntry, dKey);
            else
                throw HCMInitException(std::format("Unsupported typename passed to instantiateVector {}: {}", entryName, typeName));

        }
    }
}

// Checks for "0x" to know if it's a hex string. Also checks for negatives ("-" at start)
int64_t stringToInt(std::string& string)
{
    std::string justNumbers = string;
    bool negativeFlag = string.starts_with("-");
    if (negativeFlag) justNumbers = string.substr(1); // remove the negative sign from string
    bool hexFlag = string.starts_with("0x");
    if (hexFlag) justNumbers = justNumbers.substr(2); // remove the hex sign from string


    try
    {
        int64_t result = stoi(string, 0, hexFlag ? 16 : 10);

        if (negativeFlag) result *= -1;
        PLOG_VERBOSE << std::format("stringToInt converted {0} to {1}", string, result);
        return result;
    }
    catch (std::invalid_argument ex)
    {
        throw HCMInitException(std::format("Error parsing string to int: {}", string));
    }

}


std::vector<int64_t> getOffsetsFromXML(pugi::xml_node versionEntry)
{
    using namespace pugi;
    xml_node offsetArray = versionEntry.child("Offsets");
    std::vector<int64_t> result;

    for (xml_node offsetElement = offsetArray.first_child(); offsetElement; offsetElement = offsetElement.next_sibling())
    {
        std::string offsetString = offsetElement.text().get();
        result.emplace_back(stringToInt(offsetString));
    }

    return result;

}


void PointerManager::PointerManagerImpl::instantiateMultilevelPointer(pugi::xml_node versionEntry, std::string entryType, DataKey dKey)
{
    std::shared_ptr<MultilevelPointer> result;
    if (entryType == "MultilevelPointer::ExeOffset")
    {
        PLOG_DEBUG << "exeOffset";
        auto offsets = getOffsetsFromXML(versionEntry);
        result = MultilevelPointer::make(offsets);
    }
    else if (entryType == "MultilevelPointer::ModuleOffset")
    {
        PLOG_DEBUG << "moduleOffset";
        std::string moduleString = versionEntry.child("Module").text().get();
        auto offsets = getOffsetsFromXML(versionEntry);
        result = MultilevelPointer::make(str_to_wstr(moduleString), offsets);
    }
    else
    {
        PLOG_ERROR << "INVALID MULTILEVEL POINTER TYPE: " << entryType;
        return;
    }
    PLOG_DEBUG << "MultilevelPointer added to map: " << dKey._Myfirst._Val;
    mAllData.try_emplace(dKey, result);

}


void PointerManager::PointerManagerImpl::instantiateMidhookContextInterpreter(pugi::xml_node versionEntry, DataKey dKey)
{
    std::shared_ptr<MidhookContextInterpreter> result;
    std::vector<ParameterLocation> parameterRegisters;

    using namespace pugi;

    xml_node paramArray = versionEntry.first_child();
    for (xml_node parameter = paramArray.first_child(); parameter; parameter = parameter.next_sibling())
    {
        std::string parameterLocationText = parameter.text().as_string();

        auto mathSymbol = parameterLocationText.find_first_of('+');
        if (mathSymbol == std::string::npos)
        {
            mathSymbol = parameterLocationText.find_first_of('-');
        }
        
        std::string registerText = mathSymbol == std::string::npos ? parameterLocationText : parameterLocationText.substr(0, mathSymbol);
        PLOG_VERBOSE << "registerText: " << registerText;


        if (!stringToRegister.contains(registerText))
        {
            throw HCMInitException(std::format("invalid parameter string when parsing MidhookContextInterpreter->{}: {}", dKey._Myfirst._Val, parameterLocationText));
        }

        if (mathSymbol != std::string::npos)
        {
            PLOG_VERBOSE << "parsing RSP/RBP offset";
            // parsing the number from the string is a pain in the ass
            std::string offsetText = parameterLocationText.substr(mathSymbol, parameterLocationText.size());

            PLOG_VERBOSE << "offsetText " << offsetText;
            int offset = offsetText.contains("0x") ? stoi(offsetText, 0, 16) : stoi(offsetText);
            PLOG_VERBOSE << "offset " << offset;
            std::vector<int> offsets = { offset }; // TODO: rewrite this section to be capable of handling multiple levels of offsets
            parameterRegisters.push_back(ParameterLocation(stringToRegister.at(registerText), offsets));

        }
        else
        {
            parameterRegisters.push_back(ParameterLocation(stringToRegister.at(registerText)));
        }


    }

    if (parameterRegisters.empty())
    {
        throw HCMInitException(std::format("no parameter strings found when parsing MidhookContextInterpreter {}", dKey._Myfirst._Val));
    }

    result = std::make_shared<MidhookContextInterpreter>(parameterRegisters);

    PLOG_DEBUG << "MidhookContextInterpreter added to map: " << dKey._Myfirst._Val;
    mAllData.try_emplace(dKey, result);
   
}


template <typename T>
void PointerManager::PointerManagerImpl::instantiateVectorInteger(pugi::xml_node versionEntry, DataKey dKey)
{
    std::shared_ptr<std::vector<std::any>> out = std::make_shared<std::vector<std::any>>();

    std::string tmp;
    std::string s = versionEntry.first_child().text().as_string();
    PLOG_INFO << "instantiateVectorNumber: " << s;
    std::stringstream ss(s);


    while (std::getline(ss, tmp, ','))
    {
        // Hexadecimal conversion
        auto number = tmp.contains("0x") ? stoll(tmp, 0, 16) : stoll(tmp);
        try
        {
            out.get()->push_back((T)number);

        }
        catch (const std::bad_cast& e)
        {
            throw HCMInitException(std::format("Could not convert number to typename for entry {}: {}", dKey._Myfirst._Val, e.what()));
        }
    }

    
    mAllData.try_emplace(dKey, out);

   

}


template <typename T>
void PointerManager::PointerManagerImpl::instantiateVectorFloat(pugi::xml_node versionEntry, DataKey dKey)
{
    std::shared_ptr<std::vector<std::any>> out = std::make_shared<std::vector<std::any>>();

    std::string tmp;
    std::string s = versionEntry.first_child().text().as_string();
    PLOG_INFO << "instantiateVectorNumber: " << s;
    std::stringstream ss(s);

  
    while (std::getline(ss, tmp, ','))
    {
        // string to long double which we dynamic_cast down to T
        auto number = stold(tmp);
        try
        {

            out.get()->push_back((T)number);
        
        }
        catch (const std::bad_cast& e)
        {
            throw HCMInitException(std::format("Could not convert number to typename for entry {}: {}", dKey._Myfirst._Val, e.what()));
        }
    }
   
   

    mAllData.try_emplace(dKey, out);



}


void PointerManager::PointerManagerImpl::instantiateVectorString(pugi::xml_node versionEntry, DataKey dKey)
{
    std::shared_ptr<std::vector<std::any>> out = std::make_shared<std::vector<std::any>>();


    for (pugi::xml_node stringEntry = versionEntry.first_child(); stringEntry; stringEntry = stringEntry.next_sibling())
    {
        std::string s = stringEntry.text().as_string();
        out.get()->push_back(s);
    }

    mAllData.try_emplace(dKey, out);



}



