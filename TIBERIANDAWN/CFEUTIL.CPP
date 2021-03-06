#define WIN32_LEAN_AND_MEAN
#include <algorithm>
#include <Windows.h>
#include <ShlObj.h>
#include <string>

#include "FUNCTION.H"
#include "INI.H"
#include "RANGEWRAPPER.H"

std::string Get_User_Mod_Dir()
{
	std::string dirPath;
	char documentsPath[1024];
	HRESULT result = SHGetFolderPathA(NULL, CSIDL_MYDOCUMENTS, NULL, NULL, documentsPath);
	if (SUCCEEDED(result))
	{
		dirPath = documentsPath;
		dirPath += "\\CnCRemastered\\Mods\\Tiberian_Dawn";
	}
	return dirPath;
}

bool Load_INI_File(INIClass& ini, const char* path)
{
	CCFileClass file(path);
	if (file.Is_Available())
	{
		ini.Clear();
		return ini.Load(file);
	}
	return false;
}

void Populate_CFE_Patch_Config_From_INI(const INIClass& ini)
{
	static constexpr int InvalidSetting = std::numeric_limits<int>::lowest();
	const auto GetSetting = [&ini](auto& target, const char* section, const char* entry)
	{
		const int value = ini.Get_Int(section, entry, InvalidSetting);
		if (value != InvalidSetting)
			target = value;
	};



	//cfehunter 12/06/2020 It's really starting to get to the point where I should make feature flags
	//Features
	GetSetting(ActiveCFEPatchConfig.EnableRallyPoints         , "FEATURES", "RALLY_POINTS");
	GetSetting(ActiveCFEPatchConfig.EnableASPathing           , "FEATURES", "ASTAR_PATHING");
	GetSetting(ActiveCFEPatchConfig.EnableOOMRepair           , "FEATURES", "OOM_REPAIR");
	GetSetting(ActiveCFEPatchConfig.EnableHarvyQueueJump      , "FEATURES", "HARVY_QUEUE_JUMP");
	GetSetting(ActiveCFEPatchConfig.EnableRepairQueue         , "FEATURES", "REPAIR_QUEUE");
	GetSetting(ActiveCFEPatchConfig.EnableHarvySelfRepair     , "FEATURES", "HARVY_SELF_REPAIR");
	GetSetting(ActiveCFEPatchConfig.DisableCommandoAirstrikes , "FEATURES", "DISABLE_COMMANDO_AIRSTRIKES");
	GetSetting(ActiveCFEPatchConfig.EnableAttackMove          , "FEATURES", "ATTACK_MOVE");
	GetSetting(ActiveCFEPatchConfig.EnableCommandoGuard       , "FEATURES", "COMMANDO_GUARD");

	//Settings
	GetSetting(ActiveCFEPatchConfig.BuildingGap               , "SETTINGS", "BUILDING_GAP");
	GetSetting(ActiveCFEPatchConfig.HarvyQueueJumpCutoff      , "SETTINGS", "QUEUE_JUMP_CUTOFF");
	GetSetting(ActiveCFEPatchConfig.TibGrowthScale            , "SETTINGS", "TIB_GROWTH_SCALE");
	GetSetting(ActiveCFEPatchConfig.WallBuildLength           , "SETTINGS", "WALL_BUILD_LENGTH");
	GetSetting(ActiveCFEPatchConfig.WallsAlwaysHaveFullCost   , "SETTINGS", "WALLS_HAVE_FULL_COST");

	//Clamp Wall Length between 1 (build at all) and 10 (buffer limits)
	ActiveCFEPatchConfig.WallBuildLength = (std::max)(1, (std::min)(ActiveCFEPatchConfig.WallBuildLength, 10));
}

bool Create_CFE_Patch_Directory(const char* path)
{
	const int directory_result = SHCreateDirectoryExA(NULL, path, NULL);
	return directory_result == ERROR_SUCCESS || directory_result == ERROR_ALREADY_EXISTS;
}

bool Reconcile_Config_With_Default(const INIClass& default, INIClass& config)
{
	bool changed = false;
	using KVPair = std::pair<const char*, const char*>;
	for (const char* section : default)
	{
		for (KVPair entry : make_iterator_range(default.section_begin(section), default.section_end(section)))
		{
			if (!config.Is_Present(section, entry.first))
			{
				config.Put_String(section, entry.first, entry.second);
				changed = true;
			}
		}
	}

	return changed;
}

//cfehunter 12/06/2020 I really miss std::filesystem
void Initialise_CFE_Patch_Config()
{
	static constexpr const char* DefaultININame = "DEFAULT.CFEPATCH.INI";

	const std::string ini_dir = Get_User_Mod_Dir() + "\\CFEPatch";
	const std::string ini_path = ini_dir + "\\CFEPATCH.INI";

	INIClass default_ini;
	Load_INI_File(default_ini, DefaultININame);

	

	//Initialise from the default config
	Populate_CFE_Patch_Config_From_INI(default_ini);

	//Overwrite settings from the user config
	INIClass user_ini;
	if (Load_INI_File(user_ini, ini_path.c_str()))
	{
		Populate_CFE_Patch_Config_From_INI(user_ini);

		//Add any missing settings
		if (Reconcile_Config_With_Default(default_ini, user_ini))
		{
			//Create the target directory
			if (Create_CFE_Patch_Directory(ini_dir.c_str()))
			{
				//Write the modified ini to a temp file
				const std::string write_path = ini_path + ".temp";
				CCFileClass write_file(write_path.c_str());
				if (write_file.Is_Available() && write_file.Open(WRITE))
				{
					//If it was written successfully, overwrite the existing config
					if (user_ini.Save(write_file))
					{
						write_file.Close();
						CopyFileA(write_path.c_str(), ini_path.c_str(), FALSE);

						//Delete the old file
						DeleteFileA(write_path.c_str());
					}
				}
			}
		}
	}
	else
	{
		//Clone the default
		CCFileClass defaultConfig(DefaultININame);
		if (defaultConfig.Is_Available())
		{
			//Create directories and clone the default config
			if (Create_CFE_Patch_Directory(ini_dir.c_str()))
			{
				//Copy the default ini to the user directory so they can modify it later
				CopyFileA(defaultConfig.File_Name(), ini_path.c_str(), TRUE);
			}
		}
	}

}