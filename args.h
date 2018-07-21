#pragma once

#define ARGS_REPODIR {"repodir", 'r', 0, G_OPTION_ARG_STRING, &repodir, "repo directory", NULL}
#define ARGS_KEYDIR  {"keydir", 'k', 0, G_OPTION_ARG_STRING, &keysdir, "keys directory", NULL}

#define ARGS_ACTION_LIST   {"list", 'l', 0, G_OPTION_ARG_NONE, &action_list, "list images", NULL}
#define ARGS_ACTION_ADD    {"add", 'a', 0, G_OPTION_ARG_NONE, &action_add, "add an image", NULL}
#define ARGS_ACTION_UPDATE {"update", 'u', 0, G_OPTION_ARG_NONE, &action_update,"update an image", NULL}
#define ARGS_ACTION_DELETE {"delete", 'd', 0, G_OPTION_ARG_NONE, &action_delete,"delete an image", NULL}
#define ARGS_ACTION_VERIFY {"verify", 'v', 0, G_OPTION_ARG_NONE, &action_verify,"verify images and manifest", NULL}

#define ARGS_PARAMETER_IMAGEPATH  {"imagepath", 'p', 0, G_OPTION_ARG_FILENAME, &param_imagepath, "path to image", NULL}
#define ARGS_PARAMETER_IMAGEINDEX {"imageindex", 'i', 0, G_OPTION_ARG_INT, &param_imageindex, "index of image", NULL}
#define ARGS_PARAMETER_IMAGENAME {"imageversion", 'v', 0, G_OPTION_ARG_INT, &param_imageindex, "image version code", NULL}
