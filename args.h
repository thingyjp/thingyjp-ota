#pragma once

// for ota only
#define ARGS_HOST    {"host", 'h', 0, G_OPTION_ARG_STRING, &host,"OTA server host", NULL}
#define ARGS_PATH    {"path", 'p', 0, G_OPTION_ARG_STRING, &path,"path", NULL}
#define ARGS_MTD     {"mtd", 'm', 0, G_OPTION_ARG_STRING_ARRAY, &mtds,"mtd", NULL}
#define ARGS_DRYRUN  {"dryrun", 0, 0, G_OPTION_ARG_NONE, &dryrun,"Don't actually apply updates", NULL}
#define ARGS_FORCE   {"force", 0, 0, G_OPTION_ARG_NONE, &force,"Update even if the latest version is the same", NULL}
#define ARGS_LOG     {"logfile", 'l', 0, G_OPTION_ARG_STRING, &logfile, NULL, NULL}

// for stamp only
#define ARGS_ROOTDIR                 {"rootdir", 't', 0, G_OPTION_ARG_FILENAME, &arg_rootdir,"image root directory", NULL}
#define ARGS_PARAMETER_IMAGEVERSION  {"version", 'v', 0, G_OPTION_ARG_INT, &param_imageversion, "version of image", NULL}
#define ARGS_REPOUUID				 {"repouuid", 'u', 0, G_OPTION_ARG_STRING, &arg_repouuid, "uuid of the repo", NULL}

// for stamp and ota
#define ARGS_CONFIGDIR {"configdir", 'c', 0, G_OPTION_ARG_FILENAME, &arg_configdir,"ota config directory", NULL}

// for stamp and repo
#define ARGS_REPODIR       {"repodir", 'r', 0, G_OPTION_ARG_STRING, &arg_repodir, "repo directory", NULL}

// for repo only
#define ARGS_KEYDIR        {"keydir", 'k', 0, G_OPTION_ARG_STRING, &keysdir, "keys directory", NULL}
#define ARGS_ACTION_LIST   {"list", 0, 0, G_OPTION_ARG_NONE, &action_list, "list images", NULL}
#define ARGS_ACTION_ADD    {"add", 0, 0, G_OPTION_ARG_NONE, &action_add, "add an image", NULL}
#define ARGS_ACTION_UPDATE {"update", 0, 0, G_OPTION_ARG_NONE, &action_update,"update an image", NULL}
#define ARGS_ACTION_DELETE {"delete", 0, 0, G_OPTION_ARG_NONE, &action_delete,"delete an image", NULL}
#define ARGS_ACTION_VERIFY {"verify", 0, 0, G_OPTION_ARG_NONE, &action_verify,"verify images and manifest", NULL}
#define ARGS_ACTION_REPAIR {"repair", 0, 0, G_OPTION_ARG_NONE, &action_repair,"", NULL}

#define ARGS_PARAMETER_IMAGEPATH    {"path", 'p', 0, G_OPTION_ARG_FILENAME, &param_imagepath, "path to image", NULL}
#define ARGS_PARAMETER_IMAGEINDEX   {"index", 'i', 0, G_OPTION_ARG_INT, &param_imageindex, "index of image", NULL}
#define ARGS_PARAMETER_IMAGESTAMP	{"stamp", 's', 0, G_OPTION_ARG_FILENAME, &param_stamp, "image stamp path", NULL}
#define ARGS_PARAMETER_IMAGETAGS    {"tag", 't', 0, G_OPTION_ARG_STRING_ARRAY, &param_imagetags, "image tag, can be specified multiple times. To remove a tag prefix with -", NULL}
#define ARGS_PARAMETER_IMAGEENABLED {"enabled", 'e', 0, G_OPTION_ARG_STRING, &param_imageenabled, "image enabled", NULL}
