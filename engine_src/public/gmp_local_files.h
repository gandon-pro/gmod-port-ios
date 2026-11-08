#pragma once
#include <string>
namespace gmp {
// Direct children only: match the original local menu's non-recursive file.Find.
inline bool LocalFile(const std::string &path,const std::string &folder,const std::string &extension){
    const std::string prefix=folder+"/";
    if(path.compare(0,prefix.size(),prefix)||path.size()<=prefix.size()+extension.size())return false;
    if(path.find('/',prefix.size())!=std::string::npos||path.find("..")!=std::string::npos||path.find_first_of("\\\";\r\n:\t")!=std::string::npos||path.find('\0')!=std::string::npos)return false;
    return path.compare(path.size()-extension.size(),extension.size(),extension)==0;
}
inline bool LocalDeletable(const std::string &path){
    for(const char *folder:{"dupes","saves","demos"})for(const char *ext:{".dupe",".gms",".dem",".jpg",".png",".txt"})if(LocalFile(path,folder,ext))return true;
    return false;
}
}
