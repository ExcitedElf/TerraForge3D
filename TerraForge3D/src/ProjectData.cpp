#include "ProjectData.h"
#include <Utils.h>
#include <json.hpp>

nlohmann::json projectDatase;

std::string projectID = "";

void SetProjectId(std::string id)
{
    projectID = id;
}

std::string GetProjectId()
{
    return projectID;
}

std::string GetProjectDatabase()
{
    return projectDatase.dump(4); 
}

void RegisterProjectAsset(std::string uid, std::string path)
{
    projectDatase[uid] = path;
}

std::string GetProjectAsset(std::string id)
{
    if(projectDatase.find(id) != projectDatase.end())
        return projectDatase[id];
    return "";
}

bool ProjectAssetExists(std::string id)
{
    return (projectDatase.find(id) != projectDatase.end());
}

void SetProjectDatabase(std::string db)
{
    projectDatase = nlohmann::json::parse(db);
}

void SaveProjectDatabase()
{
    if (!PathExist(GetProjectResourcePath()))
        MkDir(GetProjectResourcePath());
    SaveToFile(GetProjectResourcePath() + "\\project_database.terr3d", projectDatase.dump());
}

std::string GetProjectResourcePath() {
    return GetExecutableDir() + "\\Data\\cache\\project_data\\project_" + GetProjectId();
}
