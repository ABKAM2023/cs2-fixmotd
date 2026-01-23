#include <stdio.h>
#include "FixMotd.h"
#include "schemasystem/schemasystem.h"
#include <fstream>
#include <string>
#include <vector>
#include <cstring>

FixMotd g_FixMotd;
PLUGIN_EXPOSE(FixMotd, g_FixMotd);
IVEngineServer2* engine = nullptr;
CGameEntitySystem* g_pGameEntitySystem = nullptr;
CEntitySystem* g_pEntitySystem = nullptr;
CGlobalVars *gpGlobals = nullptr;

IUtilsApi* g_pUtils;

CGameEntitySystem* GameEntitySystem()
{
	return g_pUtils->GetCGameEntitySystem();
}

void StartupServer()
{
	g_pGameEntitySystem = GameEntitySystem();
	g_pEntitySystem = g_pUtils->GetCEntitySystem();
	gpGlobals = g_pUtils->GetCGlobalVars();
}

bool FixMotd::Load(PluginId id, ISmmAPI* ismm, char* error, size_t maxlen, bool late)
{
	PLUGIN_SAVEVARS();

	GET_V_IFACE_CURRENT(GetEngineFactory, g_pCVar, ICvar, CVAR_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetEngineFactory, g_pSchemaSystem, ISchemaSystem, SCHEMASYSTEM_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, engine, IVEngineServer2, SOURCE2ENGINETOSERVER_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, g_pNetworkStringTableServer, INetworkStringTableContainer, INTERFACENAME_NETWORKSTRINGTABLESERVER);
	GET_V_IFACE_CURRENT(GetFileSystemFactory, g_pFullFileSystem, IFileSystem, FILESYSTEM_INTERFACE_VERSION);

	g_SMAPI->AddListener( this, this );

	return true;
}

bool FixMotd::Unload(char *error, size_t maxlen)
{
	ConVar_Unregister();
	
	return true;
}

void FixMotd::AllPluginsLoaded()
{
	char error[64];
	int ret;
	g_pUtils = (IUtilsApi *)g_SMAPI->MetaFactory(Utils_INTERFACE, &ret, NULL);
	if (ret == META_IFACE_FAILED)
	{
		g_SMAPI->Format(error, sizeof(error), "Missing Utils system plugin");
		ConColorMsg(Color(255, 0, 0, 255), "[%s] %s\n", GetLogTag(), error);
		std::string sBuffer = "meta unload "+std::to_string(g_PLID);
		engine->ServerCommand(sBuffer.c_str());
		return;
	}
	g_pUtils->StartupServer(g_PLID, StartupServer);
	g_pUtils->MapStartHook(g_PLID, [](const char* szMap) {
		g_FixMotd.OnLevelInit(szMap, nullptr, nullptr, nullptr, false, false);
	});
}

bool FixMotd::IsValidUrl(const std::string& url)
{
	return (url.rfind("http://", 0) == 0 || url.rfind("https://", 0) == 0);
}

void FixMotd::UpdateMotdTable()
{
	if (!g_pNetworkStringTableServer)
	{
		ConColorMsg(Color(255, 0, 0, 255), "[%s] Network string table container not available\n", GetLogTag());
		return;
	}

	ConVarRefAbstract hMotdFile("motdfile");
	const char* motdFileName = "motd.txt";
	if (hMotdFile.IsValidRef() && hMotdFile.IsConVarDataAvailable())
	{
		CUtlString motdValue = hMotdFile.GetString();
		if (motdValue.Length() > 0)
		{
			motdFileName = motdValue.Get();
		}
	}

	char szMotdPath[512];
	g_SMAPI->PathFormat(szMotdPath, sizeof(szMotdPath), "%s/%s", g_SMAPI->GetBaseDir(), motdFileName);

	std::ifstream file(szMotdPath);
	if (!file.is_open())
	{
		ConColorMsg(Color(255, 0, 0, 255), "[%s] MOTD file not found at %s\n", GetLogTag(), szMotdPath);
		return;
	}

	std::string url;
	std::getline(file, url);
	file.close();

	size_t start = url.find_first_not_of(" \t\r\n");
	size_t end = url.find_last_not_of(" \t\r\n");
	if (start != std::string::npos && end != std::string::npos)
	{
		url = url.substr(start, end - start + 1);
	}

	if (!IsValidUrl(url))
	{
		Msg("[%s] MOTD content is not a valid HTTP/HTTPS URL: %s\n", GetLogTag(), url.c_str());
		return;
	}

	INetworkStringTable* pTable = g_pNetworkStringTableServer->FindTable("InfoPanel");
	if (!pTable)
	{
		ConColorMsg(Color(255, 0, 0, 255), "[%s] Failed to find InfoPanel string table\n", GetLogTag());
		return;
	}

	int urlLen = (int)url.length() + 1;
	
	SetStringUserDataRequest_t userData{};
	userData.m_pRawData = const_cast<void*>(static_cast<const void*>(url.c_str()));
	userData.m_cbDataSize = static_cast<unsigned int>(urlLen);
	
	int stringIndex = pTable->FindStringIndex("motd");
	if (stringIndex == (int)INVALID_STRING_INDEX)
	{
		int result = pTable->AddString(true, "motd", &userData);
		if (result == (int)INVALID_STRING_INDEX)
		{
			ConColorMsg(Color(255, 0, 0, 255), "[%s] Failed to add MOTD string to table\n", GetLogTag());
			return;
		}
		Msg("[%s] Successfully added MOTD string with URL: %s\n", GetLogTag(), url.c_str());
	}
	else
	{
		if (pTable->SetStringUserData(stringIndex, &userData, true))
		{
			Msg("[%s] Successfully updated MOTD URL: %s\n", GetLogTag(), url.c_str());
		}
		else
		{
			ConColorMsg(Color(255, 0, 0, 255), "[%s] Failed to update MOTD URL\n", GetLogTag());
		}
	}
}

void FixMotd::OnLevelInit(char const* pMapName, char const* pMapEntities, char const* pOldLevel, char const* pLandmarkName, bool loadGame, bool background)
{
	g_pUtils->NextFrame([this]() {
		UpdateMotdTable();
	});
}

const char* FixMotd::GetLicense()
{
	return "GPL";
}

const char* FixMotd::GetVersion()
{
	return "1.0";
}

const char* FixMotd::GetDate()
{
	return __DATE__;
}

const char *FixMotd::GetLogTag()
{
	return "FixMotd";
}

const char* FixMotd::GetAuthor()
{
	return "ABKAM";
}

const char* FixMotd::GetDescription()
{
	return "FixMotd";
}

const char* FixMotd::GetName()
{
	return "FixMotd";
}

const char* FixMotd::GetURL()
{
	return "https://discord.gg/ChYfTtrtmS";
}
