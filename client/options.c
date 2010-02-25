/********************************************************************** 
 Freeciv - Copyright (C) 1996 - A Kjeldberg, L Gregersen, P Unold
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
***********************************************************************/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

/* utility */
#include "fcintl.h"
#include "hash.h"
#include "ioz.h"
#include "log.h"
#include "mem.h"
#include "registry.h"
#include "shared.h"
#include "string_vector.h"
#include "support.h"

/* common */
#include "events.h"
#include "version.h"

/* agents */
#include "cma_fec.h"

/* include */
#include "chatline_g.h"
#include "dialogs_g.h"
#include "gui_main_g.h"
#include "menu_g.h"
#include "voteinfo_bar_g.h"

/* client */
#include "audio.h"
#include "cityrepdata.h"
#include "client_main.h"
#include "global_worklist.h"
#include "mapview_common.h"
#include "overview_common.h"
#include "plrdlg_common.h"
#include "repodlgs_common.h"
#include "servers.h"
#include "themes_common.h"
#include "tilespec.h"

#include "options.h"


/** Defaults for options normally on command line **/

char default_user_name[512] = "\0";
char default_server_host[512] = "localhost";
int  default_server_port = DEFAULT_SOCK_PORT;
char default_metaserver[512] = META_URL;
char default_tileset_name[512] = "\0";
char default_sound_set_name[512] = "stdsounds";
char default_sound_plugin_name[512] = "\0";

bool save_options_on_exit = TRUE;
bool fullscreen_mode = FALSE;

/** Local Options: **/

bool solid_color_behind_units = FALSE;
bool sound_bell_at_new_turn = FALSE;
int smooth_move_unit_msec = 30;
int smooth_center_slide_msec = 200;
bool do_combat_animation = TRUE;
bool ai_manual_turn_done = TRUE;
bool auto_center_on_unit = TRUE;
bool auto_center_on_combat = FALSE;
bool auto_center_each_turn = TRUE;
bool wakeup_focus = TRUE;
bool goto_into_unknown = TRUE;
bool center_when_popup_city = TRUE;
bool concise_city_production = FALSE;
bool auto_turn_done = FALSE;
bool meta_accelerators = TRUE;
bool ask_city_name = TRUE;
bool popup_new_cities = TRUE;
bool popup_caravan_arrival = TRUE;
bool keyboardless_goto = TRUE;
bool enable_cursor_changes = TRUE;
bool separate_unit_selection = FALSE;
bool unit_selection_clears_orders = TRUE;
char highlight_our_names[128] = "yellow";

bool voteinfo_bar_use = TRUE;
bool voteinfo_bar_always_show = FALSE;
bool voteinfo_bar_hide_when_not_player = FALSE;
bool voteinfo_bar_new_at_front = FALSE;

/* This option is currently set by the client - not by the user. */
bool update_city_text_in_refresh_tile = TRUE;

bool draw_city_outlines = TRUE;
bool draw_city_output = FALSE;
bool draw_map_grid = FALSE;
bool draw_city_names = TRUE;
bool draw_city_growth = TRUE;
bool draw_city_productions = FALSE;
bool draw_city_buycost = FALSE;
bool draw_city_trade_routes = FALSE;
bool draw_terrain = TRUE;
bool draw_coastline = FALSE;
bool draw_roads_rails = TRUE;
bool draw_irrigation = TRUE;
bool draw_mines = TRUE;
bool draw_fortress_airbase = TRUE;
bool draw_specials = TRUE;
bool draw_pollution = TRUE;
bool draw_cities = TRUE;
bool draw_units = TRUE;
bool draw_focus_unit = FALSE;
bool draw_fog_of_war = TRUE;
bool draw_borders = TRUE;
bool draw_full_citybar = TRUE;
bool draw_unit_shields = TRUE;
bool player_dlg_show_dead_players = TRUE;
bool reqtree_show_icons = TRUE;
bool reqtree_curved_lines = FALSE;

/* gui-gtk-2.0 client specific options. */
char gui_gtk2_default_theme_name[512] = FC_GTK_DEFAULT_THEME_NAME;
bool gui_gtk2_map_scrollbars = FALSE;
bool gui_gtk2_dialogs_on_top = TRUE;
bool gui_gtk2_show_task_icons = TRUE;
bool gui_gtk2_enable_tabs = TRUE;
bool gui_gtk2_better_fog = TRUE;
bool gui_gtk2_show_chat_message_time = FALSE;
bool gui_gtk2_split_bottom_notebook = FALSE;
bool gui_gtk2_new_messages_go_to_top = FALSE;
bool gui_gtk2_show_message_window_buttons = TRUE;
bool gui_gtk2_metaserver_tab_first = FALSE;
bool gui_gtk2_allied_chat_only = FALSE;
bool gui_gtk2_small_display_layout = FALSE;
bool gui_gtk2_mouse_over_map_focus = FALSE;
char gui_gtk2_font_city_label[512] = "Monospace 8";
char gui_gtk2_font_notify_label[512] = "Monospace Bold 9";
char gui_gtk2_font_spaceship_label[512] = "Monospace 8";
char gui_gtk2_font_help_label[512] = "Sans Bold 10";
char gui_gtk2_font_help_link[512] = "Sans 9";
char gui_gtk2_font_help_text[512] = "Monospace 8";
char gui_gtk2_font_chatline[512] = "Monospace 8";
char gui_gtk2_font_beta_label[512] = "Sans Italic 10";
char gui_gtk2_font_small[512] = "Sans 9";
char gui_gtk2_font_comment_label[512] = "Sans Italic 9";
char gui_gtk2_font_city_names[512] = "Sans Bold 10";
char gui_gtk2_font_city_productions[512] = "Serif 10";
char gui_gtk2_font_reqtree_text[512] = "Serif 10";

/* gui-sdl client specific options. */
char gui_sdl_default_theme_name[512] = FC_SDL_DEFAULT_THEME_NAME;
bool gui_sdl_fullscreen = FALSE;
int gui_sdl_screen_width = 640;
int gui_sdl_screen_height = 480;

/* gui-win32 client specific options. */
bool gui_win32_better_fog = TRUE;
bool gui_win32_enable_alpha = TRUE;

/* Set to TRUE after the first call to options_init(), to avoid the usage
 * of non-initialized datas when calling the changed callback. */
static bool options_fully_initialized = FALSE;

static struct hash_table *settable_options_hash = NULL;
static struct hash_table *dialog_options_hash = NULL;


/****************************************************************************
  The base class for options.
****************************************************************************/
struct option {
  /* Type of the option. */
  enum option_type type;

  /* Common accessors. */
  const struct option_common_vtable {
    int (*number) (const struct option *);
    const char * (*name) (const struct option *);
    const char * (*description) (const struct option *);
    const char * (*help_text) (const struct option *);
    int (*category) (const struct option *);
    struct option * (*next) (const struct option *);
  } *common_vtable;
  /* Specific typed accessors. */
  union {
    /* Specific boolean accessors. */
    const struct option_bool_vtable {
      bool (*get) (const struct option *);
      bool (*def) (const struct option *);
      bool (*set) (struct option *, bool);
    } *bool_vtable;
    /* Specific integer accessors. */
    const struct option_int_vtable {
      int (*get) (const struct option *);
      int (*def) (const struct option *);
      int (*min) (const struct option *);
      int (*max) (const struct option *);
      bool (*set) (struct option *, int);
    } *int_vtable;
    /* Specific string accessors. */
    const struct option_str_vtable {
      const char * (*get) (const struct option *);
      const char * (*def) (const struct option *);
      const struct strvec * (*values) (const struct option *);
      bool (*set) (struct option *, const char *);
    } *str_vtable;
    /* Specific font accessors. */
    const struct option_font_vtable {
      const char * (*get) (const struct option *);
      const char * (*def) (const struct option *);
      const char * (*target) (const struct option *);
      bool (*set) (struct option *, const char *);
    } *font_vtable;
  };

  /* Called after the value changed. */
  void (*changed_callback) (struct option *option);

  /* Volatile. */
  void *gui_data;
};

#define OPTION(poption) ((struct option *) (poption))

#define OPTION_BOOL_INIT(common_table, bool_table, changed_cb) {            \
  .type = OT_BOOLEAN,                                                       \
  .common_vtable = &common_table,                                           \
  {                                                                         \
    .bool_vtable = &bool_table                                              \
  },                                                                        \
  .changed_callback = changed_cb,                                           \
  .gui_data = NULL                                                          \
}
#define OPTION_INT_INIT(common_table, int_table, changed_cb) {              \
  .type = OT_INTEGER,                                                       \
  .common_vtable = &common_table,                                           \
  {                                                                         \
    .int_vtable = &int_table                                                \
  },                                                                        \
  .changed_callback = changed_cb,                                           \
  .gui_data = NULL                                                          \
}
#define OPTION_STR_INIT(common_table, str_table, changed_cb) {              \
  .type = OT_STRING,                                                        \
  .common_vtable = &common_table,                                           \
  {                                                                         \
    .str_vtable = &str_table                                                \
  },                                                                        \
  .changed_callback = changed_cb,                                           \
  .gui_data = NULL                                                          \
}
#define OPTION_FONT_INIT(common_table, font_table, changed_cb) {            \
  .type = OT_FONT,                                                          \
  .common_vtable = &common_table,                                           \
  {                                                                         \
    .font_vtable = &font_table                                              \
  },                                                                        \
  .changed_callback = changed_cb,                                           \
  .gui_data = NULL                                                          \
}


/****************************************************************************
  Virtuals tables for the client options.
****************************************************************************/
static int client_option_number(const struct option *poption);
static const char *client_option_name(const struct option *poption);
static const char *client_option_description(const struct option *poption);
static const char *client_option_help_text(const struct option *poption);
static int client_option_category(const struct option *poption);
static struct option *client_option_next(const struct option *);

static const struct option_common_vtable client_option_common_vtable = {
  .number = client_option_number,
  .name = client_option_name,
  .description = client_option_description,
  .help_text = client_option_help_text,
  .category = client_option_category,
  .next = client_option_next
};

static bool client_option_bool_get(const struct option *poption);
static bool client_option_bool_def(const struct option *poption);
static bool client_option_bool_set(struct option *poption, bool val);

static const struct option_bool_vtable client_option_bool_vtable = {
  .get = client_option_bool_get,
  .def = client_option_bool_def,
  .set = client_option_bool_set
};

static int client_option_int_get(const struct option *poption);
static int client_option_int_def(const struct option *poption);
static int client_option_int_min(const struct option *poption);
static int client_option_int_max(const struct option *poption);
static bool client_option_int_set(struct option *poption, int val);

static const struct option_int_vtable client_option_int_vtable = {
  .get = client_option_int_get,
  .def = client_option_int_def,
  .min = client_option_int_min,
  .max = client_option_int_max,
  .set = client_option_int_set
};

static const char *client_option_str_get(const struct option *poption);
static const char *client_option_str_def(const struct option *poption);
static const struct strvec *
    client_option_str_values(const struct option *poption);
static bool client_option_str_set(struct option *poption, const char *str);

static const struct option_str_vtable client_option_str_vtable = {
  .get = client_option_str_get,
  .def = client_option_str_def,
  .values = client_option_str_values,
  .set = client_option_str_set
};

static const char *client_option_font_get(const struct option *poption);
static const char *client_option_font_def(const struct option *poption);
static const char *client_option_font_target(const struct option *poption);
static bool client_option_font_set(struct option *poption, const char *font);

static const struct option_font_vtable client_option_font_vtable = {
  .get = client_option_font_get,
  .def = client_option_font_def,
  .target = client_option_font_target,
  .set = client_option_font_set
};

enum client_option_category {
  COC_GRAPHICS,
  COC_OVERVIEW,
  COC_SOUND,
  COC_INTERFACE,
  COC_NETWORK,
  COC_FONT,
  COC_MAX
};

/****************************************************************************
  Derived class client option, inherinting of base class option.
****************************************************************************/
struct client_option {
  struct option base_option;    /* Base structure, must be the first! */

  const char *name;             /* Short name - used as an identifier */
  const char *description;      /* One-line description */
  const char *help_text;        /* Paragraph-length help text */
  enum client_option_category category;
  enum gui_type specific;       /* GUI_LAST for common options. */

  union {
    /* OT_BOOLEAN type option. */
    struct {
      bool *const pvalue;
      const bool def;
    } boolean;
    /* OT_INTEGER type option. */
    struct {
      int *const pvalue;
      const int def, min, max;
    } integer;
    /* OT_STRING type option. */
    struct {
      char *const pvalue;
      const size_t size;
      const char *const def;
      /* 
       * A function to return a string vector of possible string values,
       * or NULL for none. 
       */
      const struct strvec *(*const val_accessor) (void);
    } string;
    /* OT_FONT type option. */
    struct {
      char *const pvalue;
      const size_t size;
      const char *const def;
      const char *const target;
    } font;
  };
};

#define CLIENT_OPTION(poption) ((struct client_option *) (poption))

/*
 * Generate a client option of type OT_BOOLEAN.
 *
 * oname: The option data.  Note it is used as name to be loaded or saved.
 *        So, you shouldn't change the name of this variable in any case.
 * odesc: A short description of the client option.  Should be used with the
 *        N_() macro.
 * ohelp: The help text for the client option.  Should be used with the N_()
 *        macro.
 * ocat:  The client_option_class of this client option.
 * ospec: A gui_type enumerator which determin for what particular client
 *        gui this option is for.  Sets to GUI_LAST for common options.
 * odef:  The default value of this client option (FALSE or TRUE).
 * ocb:   A callback function of type void (*)(struct option *) called when
 *        the option changed.
 */
#define GEN_BOOL_OPTION(oname, odesc, ohelp, ocat, ospec, odef, ocb)        \
{                                                                           \
  .base_option = OPTION_BOOL_INIT(client_option_common_vtable,              \
                                  client_option_bool_vtable, ocb),          \
  .name = #oname,                                                           \
  .description = odesc,                                                     \
  .help_text = ohelp,                                                       \
  .category = ocat,                                                         \
  .specific = ospec,                                                        \
  {                                                                         \
    .boolean = {                                                            \
      .pvalue = &oname,                                                     \
      .def = odef,                                                          \
    }                                                                       \
  },                                                                        \
}

/*
 * Generate a client option of type OT_INTEGER.
 *
 * oname: The option data.  Note it is used as name to be loaded or saved.
 *        So, you shouldn't change the name of this variable in any case.
 * odesc: A short description of the client option.  Should be used with the
 *        N_() macro.
 * ohelp: The help text for the client option.  Should be used with the N_()
 *        macro.
 * ocat:  The client_option_class of this client option.
 * ospec: A gui_type enumerator which determin for what particular client
 *        gui this option is for.  Sets to GUI_LAST for common options.
 * odef:  The default value of this client option.
 * omin:  The minimal value of this client option.
 * omax:  The maximal value of this client option.
 * ocb:   A callback function of type void (*)(struct option *) called when
 *        the option changed.
 */
#define GEN_INT_OPTION(oname, odesc, ohelp, ocat, ospec, odef, omin, omax, ocb) \
{                                                                           \
  .base_option = OPTION_INT_INIT(client_option_common_vtable,               \
                                 client_option_int_vtable, ocb),            \
  .name = #oname,                                                           \
  .description = odesc,                                                     \
  .help_text = ohelp,                                                       \
  .category = ocat,                                                         \
  .specific = ospec,                                                        \
  {                                                                         \
    .integer = {                                                            \
      .pvalue = &oname,                                                     \
      .def = odef,                                                          \
      .min = omin,                                                          \
      .max = omax                                                           \
    }                                                                       \
  },                                                                        \
}

/*
 * Generate a client option of type OT_STRING.
 *
 * oname: The option data.  Note it is used as name to be loaded or saved.
 *        So, you shouldn't change the name of this variable in any case.
 *        Be sure to pass the array variable and not a pointer to it because
 *        the size is calculated with sizeof().
 * odesc: A short description of the client option.  Should be used with the
 *        N_() macro.
 * ohelp: The help text for the client option.  Should be used with the N_()
 *        macro.
 * ocat:  The client_option_class of this client option.
 * ospec: A gui_type enumerator which determin for what particular client
 *        gui this option is for.  Sets to GUI_LAST for common options.
 * odef:  The default string for this client option.
 * ocb:   A callback function of type void (*)(struct option *) called when
 *        the option changed.
 */
#define GEN_STR_OPTION(oname, odesc, ohelp, ocat, ospec, odef, ocb)         \
{                                                                           \
  .base_option = OPTION_STR_INIT(client_option_common_vtable,               \
                                 client_option_str_vtable, ocb),            \
  .name = #oname,                                                           \
  .description = odesc,                                                     \
  .help_text = ohelp,                                                       \
  .category = ocat,                                                         \
  .specific = ospec,                                                        \
  {                                                                         \
    .string = {                                                             \
      .pvalue = oname,                                                      \
      .size = sizeof(oname),                                                \
      .def = odef,                                                          \
      .val_accessor = NULL                                                  \
    }                                                                       \
  },                                                                        \
}

/*
 * Generate a client option of type OT_STRING with a string accessor
 * function.
 *
 * oname: The option data.  Note it is used as name to be loaded or saved.
 *        So, you shouldn't change the name of this variable in any case.
 *        Be sure to pass the array variable and not a pointer to it because
 *        the size is calculated with sizeof().
 * odesc: A short description of the client option.  Should be used with the
 *        N_() macro.
 * ohelp: The help text for the client option.  Should be used with the N_()
 *        macro.
 * ocat:  The client_option_class of this client option.
 * ospec: A gui_type enumerator which determin for what particular client
 *        gui this option is for.  Sets to GUI_LAST for common options.
 * odef:  The default string for this client option.
 * oacc:  The string accessor where to find the allowed values of type
 *        const char **(*)(void) (returns a NULL-termined list of strings).
 * ocb:   A callback function of type void (*)(struct option *) called when
 *        the option changed.
 */
#define GEN_STR_LIST_OPTION(oname, odesc, ohelp, ocat, ospec, odef, oacc, ocb) \
{                                                                           \
  .base_option = OPTION_STR_INIT(client_option_common_vtable,               \
                                 client_option_str_vtable, ocb),            \
  .name = #oname,                                                           \
  .description = odesc,                                                     \
  .help_text = ohelp,                                                       \
  .category = ocat,                                                         \
  .specific = ospec,                                                        \
  {                                                                         \
    .string = {                                                             \
      .pvalue = oname,                                                      \
      .size = sizeof(oname),                                                \
      .def = odef,                                                          \
      .val_accessor = oacc                                                  \
    }                                                                       \
  },                                                                        \
}

/*
 * Generate a client option of type OT_FONT.
 *
 * oname: The option data.  Note it is used as name to be loaded or saved.
 *        So, you shouldn't change the name of this variable in any case.
 *        Be sure to pass the array variable and not a pointer to it because
 *        the size is calculated with sizeof().
 * otgt:  The target widget style.
 * odesc: A short description of the client option.  Should be used with the
 *        N_() macro.
 * ohelp: The help text for the client option.  Should be used with the N_()
 *        macro.
 * ocat:  The client_option_class of this client option.
 * ospec: A gui_type enumerator which determin for what particular client
 *        gui this option is for.  Sets to GUI_LAST for common options.
 * odef:  The default string for this client option.
 * ocb:   A callback function of type void (*)(struct option *) called when
 *        the option changed.
 */
#define GEN_FONT_OPTION(oname, otgt, odesc, ohelp, ocat, ospec, odef, ocb)  \
{                                                                           \
  .base_option = OPTION_FONT_INIT(client_option_common_vtable,              \
                                  client_option_font_vtable, ocb),          \
  .name = #oname,                                                           \
  .description = odesc,                                                     \
  .help_text = ohelp,                                                       \
  .category = ocat,                                                         \
  .specific = ospec,                                                        \
  {                                                                         \
    .font = {                                                               \
      .pvalue = oname,                                                      \
      .size = sizeof(oname),                                                \
      .def = odef,                                                          \
      .target = otgt,                                                       \
    }                                                                       \
  },                                                                        \
}

/* Some changed callbacks. */
static void reqtree_show_icons_callback(struct option *poption);
static void view_option_changed_callback(struct option *poption);
static void mapview_redraw_callback(struct option *poption);
static void voteinfo_bar_callback(struct option *poption);
static void font_changed_callback(struct option *poption);

static struct client_option client_options[] = {
  GEN_STR_OPTION(default_user_name,
                 N_("Login name"),
                 N_("This is the default login username that will be used "
                    "in the connection dialogs or with the -a command-line "
                    "parameter."),
                 COC_NETWORK, GUI_LAST, NULL, NULL),
  GEN_STR_OPTION(default_server_host,
                 N_("Server"),
                 N_("This is the default server hostname that will be used "
                    "in the connection dialogs or with the -a command-line "
                    "parameter."),
                 COC_NETWORK, GUI_LAST, "localhost", NULL),
  GEN_INT_OPTION(default_server_port,
                 N_("Server port"),
                 N_("This is the default server port that will be used "
                    "in the connection dialogs or with the -a command-line "
                    "parameter."),
                 COC_NETWORK, GUI_LAST, DEFAULT_SOCK_PORT, 0, 65535, NULL),
  GEN_STR_OPTION(default_metaserver,
                 N_("Metaserver"),
                 N_("The metaserver is a host that the client contacts to "
                    "find out about games on the internet.  Don't change "
                    "this from its default value unless you know what "
                    "you're doing."),
                 COC_NETWORK, GUI_LAST, META_URL, NULL),
  GEN_STR_LIST_OPTION(default_sound_set_name,
                      N_("Soundset"),
                      N_("This is the soundset that will be used.  Changing "
                         "this is the same as using the -S command-line "
                         "parameter."),
                      COC_SOUND, GUI_LAST, "stdsounds", get_soundset_list, NULL),
  GEN_STR_LIST_OPTION(default_sound_plugin_name,
                      N_("Sound plugin"),
                      N_("If you have a problem with sound, try changing "
                         "the sound plugin.  The new plugin won't take "
                         "effect until you restart Freeciv.  Changing this "
                         "is the same as using the -P command-line option."),
                      COC_SOUND, GUI_LAST, NULL, get_soundplugin_list, NULL),
  /* gui_gtk2_default_theme_name and gui_sdl_default_theme_name are
   * different settings to avoid client crash after loading the
   * style for the other gui.  Keeps 2 different options! */
  GEN_STR_LIST_OPTION(gui_gtk2_default_theme_name, N_("Theme"),
                      N_("By changing this option you change the "
                         "active theme."),
                      COC_GRAPHICS, GUI_GTK2, FC_GTK_DEFAULT_THEME_NAME,
                      get_themes_list, theme_reread_callback),
  GEN_STR_LIST_OPTION(gui_sdl_default_theme_name, N_("Theme"),
                      N_("By changing this option you change the "
                         "active theme."),
                      COC_GRAPHICS, GUI_SDL, FC_SDL_DEFAULT_THEME_NAME,
                      get_themes_list, theme_reread_callback),
  GEN_STR_LIST_OPTION(default_tileset_name, N_("Tileset"),
                      N_("By changing this option you change the active "
                         "tileset.  This is the same as using the -t "
                         "command-line parameter."),
                      COC_GRAPHICS, GUI_LAST, NULL,
                      get_tileset_list, tilespec_reread_callback),

  GEN_BOOL_OPTION(draw_city_outlines, N_("Draw city outlines"),
                  N_("Setting this option will draw a line at the city "
                     "workable limit."),
                  COC_GRAPHICS, GUI_LAST, TRUE,
                  view_option_changed_callback),
  GEN_BOOL_OPTION(draw_city_output, N_("Draw city output"),
                  N_("Setting this option will draw city output for every "
                     "citizen."),
                  COC_GRAPHICS, GUI_LAST, FALSE,
                  view_option_changed_callback),
  GEN_BOOL_OPTION(draw_map_grid, N_("Draw the map grid"),
                  N_("Setting this option will draw a grid over the map."),
                  COC_GRAPHICS, GUI_LAST, FALSE,
                  view_option_changed_callback),
  GEN_BOOL_OPTION(draw_full_citybar, N_("Draw the citybar"),
                  N_("Setting this option will display a 'citybar' "
                     "containing useful information beneath each city. "
                     "Disabling this option will display only the city's "
                     "name and optionally, production."),
                  COC_GRAPHICS, GUI_LAST,
                  TRUE, view_option_changed_callback),
  GEN_BOOL_OPTION(draw_city_names, N_("Draw the city names"),
                  N_("Setting this option will draw the names of the cities "
                     "on the map."),
                  COC_GRAPHICS, GUI_LAST, TRUE,
                  view_option_changed_callback),
  GEN_BOOL_OPTION(draw_city_growth, N_("Draw the city growth"),
                  N_("Setting this option will draw in how many turns the "
                     "cities will grow or shrink."),
                  COC_GRAPHICS, GUI_LAST, TRUE,
                  view_option_changed_callback),
  GEN_BOOL_OPTION(draw_city_productions, N_("Draw the city productions"),
                  N_("Setting this option will draw what the cities are "
                     "currently building on the map."),
                  COC_GRAPHICS, GUI_LAST, FALSE,
                  view_option_changed_callback),
  GEN_BOOL_OPTION(draw_city_buycost, N_("Draw the city buy costs"),
                  N_("Setting this option will draw how many golds are "
                     "needed to buy the production of the cities."),
                  COC_GRAPHICS, GUI_LAST, FALSE,
                  view_option_changed_callback),
  GEN_BOOL_OPTION(draw_city_trade_routes, N_("Draw the city trade routes"),
                  N_("Setting this option will draw trade route lines "
                     "between cities which have trade routes."),
                  COC_GRAPHICS, GUI_LAST, FALSE,
                  view_option_changed_callback),
  GEN_BOOL_OPTION(draw_terrain, N_("Draw the terrain"),
                  N_("Setting this option will draw the terrain."),
                  COC_GRAPHICS, GUI_LAST, TRUE,
                  view_option_changed_callback),
  GEN_BOOL_OPTION(draw_coastline, N_("Draw the coast line"),
                  N_("Setting this option will draw a line to separate the "
                     "land of the ocean."),
                  COC_GRAPHICS, GUI_LAST, FALSE,
                  view_option_changed_callback),
  GEN_BOOL_OPTION(draw_roads_rails, N_("Draw the roads and the railroads"),
                  N_("Setting this option will draw the roads and the "
                     "railroads on  the map."),
                  COC_GRAPHICS, GUI_LAST, TRUE,
                  view_option_changed_callback),
  GEN_BOOL_OPTION(draw_irrigation, N_("Draw the irrigations"),
                  N_("Setting this option will draw the irrigations "
                     "on the map."),
                  COC_GRAPHICS, GUI_LAST, TRUE,
                  view_option_changed_callback),
  GEN_BOOL_OPTION(draw_mines, N_("Draw the mines"),
                  N_("Setting this option will draw the mines on the map."),
                  COC_GRAPHICS, GUI_LAST, TRUE,
                  view_option_changed_callback),
  GEN_BOOL_OPTION(draw_fortress_airbase, N_("Draw the bases"),
                  N_("Setting this option will draw the bases on the map."),
                  COC_GRAPHICS, GUI_LAST, TRUE,
                  view_option_changed_callback),
  GEN_BOOL_OPTION(draw_specials, N_("Draw the specials"),
                  N_("Setting this option will draw the specials on the "
                     "map."),
                  COC_GRAPHICS, GUI_LAST, TRUE,
                  view_option_changed_callback),
  GEN_BOOL_OPTION(draw_pollution, N_("Draw the pollution/nuclear fallouts"),
                  N_("Setting this option will draw the pollution and the "
                     "nuclear fallouts on the map."),
                  COC_GRAPHICS, GUI_LAST, TRUE,
                  view_option_changed_callback),
  GEN_BOOL_OPTION(draw_cities, N_("Draw the cities"),
                  N_("Setting this option will draw the cities on the map."),
                  COC_GRAPHICS, GUI_LAST, TRUE,
                  view_option_changed_callback),
  GEN_BOOL_OPTION(draw_units, N_("Draw the units"),
                  N_("Setting this option will draw the units on the map."),
                  COC_GRAPHICS, GUI_LAST, TRUE,
                  view_option_changed_callback),
  GEN_BOOL_OPTION(solid_color_behind_units,
                  N_("Solid unit background color"),
                  N_("Setting this option will cause units on the map "
                     "view to be drawn with a solid background color "
                     "instead of the flag backdrop."),
                  COC_GRAPHICS, GUI_LAST,
                  FALSE, view_option_changed_callback),
  GEN_BOOL_OPTION(draw_unit_shields, N_("Draw shield graphics for units"),
                  N_("Setting this option will draw a shield icon "
                     "as the flags on units.  If unset, the full flag will "
                     "be drawn."),
                  COC_GRAPHICS, GUI_LAST, TRUE, view_option_changed_callback),
  GEN_BOOL_OPTION(draw_focus_unit, N_("Draw the units in focus"),
                  N_("Setting this option will draw the units in focus, "
                     "including the case the other units wouldn't be "
                     "drawn."),
                  COC_GRAPHICS, GUI_LAST, FALSE,
                  view_option_changed_callback),
  GEN_BOOL_OPTION(draw_fog_of_war, N_("Draw the fog of war"),
                  N_("Setting this option will draw the fog of war."),
                  COC_GRAPHICS, GUI_LAST, TRUE,
                  view_option_changed_callback),
  GEN_BOOL_OPTION(draw_borders, N_("Draw the borders"),
                  N_("Setting this option will draw the national borders."),
                  COC_GRAPHICS, GUI_LAST, TRUE,
                  view_option_changed_callback),
  GEN_BOOL_OPTION(player_dlg_show_dead_players,
                  N_("Show dead players in nation report."),
                  N_("Setting this option will draw the players already "
                     "dead in the nation report page."),
                  COC_GRAPHICS, GUI_LAST, TRUE,
                  view_option_changed_callback),
  GEN_BOOL_OPTION(sound_bell_at_new_turn, N_("Sound bell at new turn"),
                  N_("Set this option to have a \"bell\" event be generated "
                     "at the start of a new turn.  You can control the "
                     "behavior of the \"bell\" event by editing the message "
                     "options."),
                  COC_SOUND, GUI_LAST, FALSE, NULL),
  GEN_INT_OPTION(smooth_move_unit_msec,
                 N_("Unit movement animation time (milliseconds)"),
                 N_("This option controls how long unit \"animation\" takes "
                    "when a unit moves on the map view.  Set it to 0 to "
                    "disable animation entirely."),
                 COC_GRAPHICS, GUI_LAST, 30, 0, 2000, NULL),
  GEN_INT_OPTION(smooth_center_slide_msec,
                 N_("Mapview recentering time (milliseconds)"),
                 N_("When the map view is recentered, it will slide "
                    "smoothly over the map to its new position.  This "
                    "option controls how long this slide lasts.  Set it to "
                    "0 to disable mapview sliding entirely."),
                 COC_GRAPHICS, GUI_LAST, 200, 0, 5000, NULL),
  GEN_BOOL_OPTION(do_combat_animation, N_("Show combat animation"),
                  N_("Disabling this option will turn off combat animation "
                     "between units on the mapview."),
                  COC_GRAPHICS, GUI_LAST, TRUE, NULL),
  GEN_BOOL_OPTION(reqtree_show_icons,
                  N_("Show icons in the technology tree"),
                  N_("Setting this option will display icons "
                     "on the technology tree diagram. Turning "
                     "this option off makes the technology tree "
                     "more compact."),
                  COC_GRAPHICS, GUI_LAST, TRUE, reqtree_show_icons_callback),
  GEN_BOOL_OPTION(reqtree_curved_lines,
                  N_("Use curved lines in the technology tree"),
                  N_("Setting this option make the technology tree "
                     "diagram use curved lines to show technology "
                     "relations. Turning this option off causes "
                     "the lines to be drawn straight."),
                  COC_GRAPHICS, GUI_LAST, FALSE,
                  reqtree_show_icons_callback),
   GEN_STR_OPTION(highlight_our_names,
                  N_("Color to highlight your player/user name"),
                  N_("If set, your player and user name in the new chat "
                     "messages will be highlighted using this color as "
                     "background.  If not set, it will just not highlight "
                     "anything."),
                  COC_GRAPHICS, GUI_LAST, "yellow", NULL),
  GEN_BOOL_OPTION(ai_manual_turn_done, N_("Manual Turn Done in AI Mode"),
                  N_("Disable this option if you do not want to "
                     "press the Turn Done button manually when watching "
                     "an AI player."),
                  COC_INTERFACE, GUI_LAST, TRUE, NULL),
  GEN_BOOL_OPTION(auto_center_on_unit, N_("Auto Center on Units"),
                  N_("Set this option to have the active unit centered "
                     "automatically when the unit focus changes."),
                  COC_INTERFACE, GUI_LAST, TRUE, NULL),
  GEN_BOOL_OPTION(auto_center_on_combat, N_("Auto Center on Combat"),
                  N_("Set this option to have any combat be centered "
                     "automatically.  Disabled this will speed up the time "
                     "between turns but may cause you to miss combat "
                     "entirely."),
                  COC_INTERFACE, GUI_LAST, FALSE, NULL),
  GEN_BOOL_OPTION(auto_center_each_turn, N_("Auto Center on New Turn"),
                  N_("Set this option to have the client automatically "
                     "recenter the map on a suitable location at the "
                     "start of each turn."),
                  COC_INTERFACE, GUI_LAST, TRUE, NULL),
  GEN_BOOL_OPTION(wakeup_focus, N_("Focus on Awakened Units"),
                  N_("Set this option to have newly awoken units be "
                     "focused automatically."),
                  COC_INTERFACE, GUI_LAST, TRUE, NULL),
  GEN_BOOL_OPTION(keyboardless_goto, N_("Keyboardless goto"),
                  N_("If this option is set then a goto may be initiated "
                     "by left-clicking and then holding down the mouse "
                     "button while dragging the mouse onto a different "
                     "tile."),
                  COC_INTERFACE, GUI_LAST, TRUE, NULL),
  GEN_BOOL_OPTION(goto_into_unknown, N_("Allow goto into the unknown"),
                  N_("Setting this option will make the game consider "
                     "moving into unknown tiles.  If not, then goto routes "
                     "will detour around or be blocked by unknown tiles."),
                  COC_INTERFACE, GUI_LAST, TRUE, NULL),
  GEN_BOOL_OPTION(center_when_popup_city, N_("Center map when Popup city"),
                  N_("Setting this option makes the mapview center on a "
                     "city when its city dialog is popped up."),
                  COC_INTERFACE, GUI_LAST, TRUE, NULL),
  GEN_BOOL_OPTION(concise_city_production, N_("Concise City Production"),
                  N_("Set this option to make the city production (as shown "
                     "in the city dialog) to be more compact."),
                  COC_INTERFACE, GUI_LAST, FALSE, NULL),
  GEN_BOOL_OPTION(auto_turn_done, N_("End Turn when done moving"),
                  N_("Setting this option makes your turn end automatically "
                     "when all your units are done moving."),
                  COC_INTERFACE, GUI_LAST, FALSE, NULL),
  GEN_BOOL_OPTION(ask_city_name, N_("Prompt for city names"),
                  N_("Disabling this option will make the names of newly "
                     "founded cities chosen automatically by the server."),
                  COC_INTERFACE, GUI_LAST, TRUE, NULL),
  GEN_BOOL_OPTION(popup_new_cities, N_("Pop up city dialog for new cities"),
                  N_("Setting this option will pop up a newly-founded "
                     "city's city dialog automatically."),
                  COC_INTERFACE, GUI_LAST, TRUE, NULL),
  GEN_BOOL_OPTION(popup_caravan_arrival, N_("Pop up caravan actions"),
                  N_("If this option is enabled, when caravans arrive "
                     "at a city where they can establish a trade route "
                     "or help build a wonder, a window will popup asking "
                     "which action should be performed. Disabling this "
                     "option means you will have to do the action "
                     "manually by pressing either 'r' (for a trade route) "
                     "or 'b' (for building a wonder) when the caravan "
                     "is in the city."),
                  COC_INTERFACE, GUI_LAST, TRUE, NULL),
  GEN_BOOL_OPTION(enable_cursor_changes, N_("Enable cursor changing"),
                  N_("This option controls whether the client should "
                     "try to change the mouse cursor depending on what "
                     "is being pointed at, as well as to indicate "
                     "changes in the client or server state."),
                  COC_INTERFACE, GUI_LAST, TRUE, NULL),
  GEN_BOOL_OPTION(separate_unit_selection, N_("Select cities before units"),
                  N_("If this option is enabled, when both cities and "
                     "units are present in the selection rectangle, only "
                     "cities will be selected."),
                  COC_INTERFACE, GUI_LAST, FALSE, NULL),
  GEN_BOOL_OPTION(unit_selection_clears_orders,
                  N_("Clear unit orders on selection"),
                  N_("Enabling this option will cause unit orders to be "
                     "cleared as soon as one or more units are selected. If "
                     "this option is disabled, busy units will not stop "
                     "their current activity when selected. Giving them "
                     "new orders will clear their current ones; pressing "
                     "<space> once will clear their orders and leave them "
                     "selected, and pressing <space> a second time will "
                     "dismiss them."),
                  COC_INTERFACE, GUI_LAST, TRUE, NULL),
  GEN_BOOL_OPTION(voteinfo_bar_use, N_("Enable vote bar"),
                  N_("If this option is turned on, the vote bar will be "
                     "displayed to show vote information."),
                  COC_GRAPHICS, GUI_LAST, TRUE, voteinfo_bar_callback),
  GEN_BOOL_OPTION(voteinfo_bar_always_show,
                  N_("Always display the vote bar"),
                  N_("If this option is turned on, the vote bar will never "
                     "be hidden, notably when there won't be any running "
                     "vote."),
                  COC_GRAPHICS, GUI_LAST, FALSE, voteinfo_bar_callback),
  GEN_BOOL_OPTION(voteinfo_bar_hide_when_not_player,
                  N_("Do not show vote bar if not a player"),
                  N_("If this option is enabled, the client won't show the "
                     "vote bar if you are not a player."),
                  COC_GRAPHICS, GUI_LAST, FALSE, voteinfo_bar_callback),
  GEN_BOOL_OPTION(voteinfo_bar_new_at_front, N_("Set new votes at front"),
                  N_("If this option is enabled, then the new votes will go "
                     "to the front of the vote list"),
                  COC_GRAPHICS, GUI_LAST, FALSE, voteinfo_bar_callback),

  GEN_BOOL_OPTION(overview.layers[OLAYER_BACKGROUND],
                  N_("Background layer"),
                  N_("The background layer of the overview shows just "
                     "ocean and land."),
                  COC_OVERVIEW, GUI_LAST, TRUE, NULL),
  GEN_BOOL_OPTION(overview.layers[OLAYER_RELIEF],
                  N_("Terrain relief map layer"),
                  N_("The relief layer shows all terrains on the map."),
                  COC_OVERVIEW, GUI_LAST, FALSE, overview_redraw_callback),
  GEN_BOOL_OPTION(overview.layers[OLAYER_BORDERS],
                  N_("Borders layer"),
                  N_("The borders layer of the overview shows which tiles "
                     "are owned by each player."),
                  COC_OVERVIEW, GUI_LAST, FALSE, overview_redraw_callback),
  GEN_BOOL_OPTION(overview.layers[OLAYER_BORDERS_ON_OCEAN],
                  N_("Borders layer on ocean tiles"),
                  N_("The borders layer of the overview are drawn on "
                     "ocean tiles as well (this may look ugly with many "
                     "islands). This option is only of interest if you "
                     "have set the option \"Borders layer\" already."),
                  COC_OVERVIEW, GUI_LAST, TRUE, overview_redraw_callback),
  GEN_BOOL_OPTION(overview.layers[OLAYER_UNITS],
                  N_("Units layer"),
                  N_("Enabling this will draw units on the overview."),
                  COC_OVERVIEW, GUI_LAST, TRUE, overview_redraw_callback),
  GEN_BOOL_OPTION(overview.layers[OLAYER_CITIES],
                  N_("Cities layer"),
                  N_("Enabling this will draw cities on the overview."),
                  COC_OVERVIEW, GUI_LAST, TRUE, overview_redraw_callback),
  GEN_BOOL_OPTION(overview.fog,
                  N_("Overview fog of war"),
                  N_("Enabling this will show fog of war on the "
                     "overview."),
                  COC_OVERVIEW, GUI_LAST, TRUE, overview_redraw_callback),

  /* gui-gtk-2.0 client specific options. */
  GEN_BOOL_OPTION(gui_gtk2_map_scrollbars, N_("Show Map Scrollbars"),
                  N_("Disable this option to hide the scrollbars on the "
                     "map view."),
                  COC_INTERFACE, GUI_GTK2, FALSE, NULL),
  GEN_BOOL_OPTION(gui_gtk2_dialogs_on_top, N_("Keep dialogs on top"),
                  N_("If this option is set then dialog windows will always "
                     "remain in front of the main Freeciv window. "
                     "Disabling this has no effect in fullscreen mode."),
                  COC_INTERFACE, GUI_GTK2, TRUE, NULL),
  GEN_BOOL_OPTION(gui_gtk2_show_task_icons, N_("Show worklist task icons"),
                  N_("Disabling this will turn off the unit and building "
                     "icons in the worklist dialog and the production "
                     "tab of the city dialog."),
                  COC_GRAPHICS, GUI_GTK2, TRUE, NULL),
  GEN_BOOL_OPTION(gui_gtk2_enable_tabs, N_("Enable status report tabs"),
                  N_("If this option is enabled then report dialogs will "
                     "be shown as separate tabs rather than in popup "
                     "dialogs."),
                  COC_INTERFACE, GUI_GTK2, TRUE, NULL),
  GEN_BOOL_OPTION(gui_gtk2_better_fog,
                  N_("Better fog-of-war drawing"),
                  N_("If this is enabled then a better method is used "
                     "for drawing fog-of-war.  It is not any slower but "
                     "will consume about twice as much memory."),
                  COC_GRAPHICS, GUI_GTK2,
                  TRUE, view_option_changed_callback),
  GEN_BOOL_OPTION(gui_gtk2_show_chat_message_time,
                  N_("Show time for each chat message"),
                  N_("If this option is enabled then all chat messages "
                     "will be prefixed by a time string of the form "
                     "[hour:minute:second]."),
                  COC_INTERFACE, GUI_GTK2, FALSE, NULL),
  GEN_BOOL_OPTION(gui_gtk2_split_bottom_notebook,
                  N_("Split bottom notebook area"),
                  N_("Enabling this option will split the bottom "
                     "notebook into a left and right notebook so that "
                     "two tabs may be viewed at once."),
                  COC_INTERFACE, GUI_GTK2, FALSE, NULL),
  GEN_BOOL_OPTION(gui_gtk2_new_messages_go_to_top,
                  N_("New message events go to top of list"),
                  N_("If this option is enabled, new events in the "
                     "message window will appear at the top of the list, "
                     "rather than being appended at the bottom."),
                  COC_INTERFACE, GUI_GTK2, FALSE, NULL),
  GEN_BOOL_OPTION(gui_gtk2_show_message_window_buttons,
                  N_("Show extra message window buttons"),
                  N_("If this option is enabled, there will be two "
                     "buttons displayed in the message window for "
                     "inspecting a city and going to a location. If this "
                     "option is disabled, these buttons will not appear "
                     "(you can still double-click with the left mouse "
                     "button or right-click on a row to inspect or goto "
                     "respectively). This option will only take effect "
                     "once the message window is closed and reopened."),
                  COC_INTERFACE, GUI_GTK2, TRUE, NULL),
  GEN_BOOL_OPTION(gui_gtk2_metaserver_tab_first,
                  N_("Metaserver tab first in network page"),
                  N_("If this option is enabled, the metaserver tab will "
                     "be the first notebook tab in the network page. This "
                     "option requires a restart in order to take effect."),
                  COC_NETWORK, GUI_GTK2, FALSE, NULL),
  GEN_BOOL_OPTION(gui_gtk2_allied_chat_only,
                  N_("Plain chat messages are sent to allies only"),
                  N_("If this option is enabled, then plain messages "
                     "typed into the chat entry while the game is "
                     "running will only be sent to your allies. "
                     "Otherwise plain messages will be sent as "
                     "public chat messages. To send a public chat "
                     "message with this option enabled, prefix the "
                     "message with a single colon ':'. This option "
                     "can also be set using a toggle button beside "
                     "the chat entry (only visible in multiplayer "
                     "games)."),
                  COC_INTERFACE, GUI_GTK2, FALSE, NULL),
  GEN_BOOL_OPTION(gui_gtk2_small_display_layout,
                  N_("Arrange widgets for small displays"),
                  N_("If this option is enabled, widgets in the main "
                     "window will be arrange so that they take up the "
                     "least amount of total screen space. Specifically, "
                     "the left panel containing the overview, player "
                     "status, and the unit information box will be "
                     "extended over the entire left side of the window. "
                     "This option requires a restart in order to take "
                     "effect."), COC_INTERFACE, GUI_GTK2, FALSE, NULL),
  GEN_BOOL_OPTION(gui_gtk2_mouse_over_map_focus,
                  N_("Mouse over the map widget selects it automatically"),
                  N_("If this option is enabled, then the map will be "
                     "focused when the mouse will be floating over it."),
                  COC_INTERFACE, GUI_GTK2, FALSE, NULL),
  GEN_FONT_OPTION(gui_gtk2_font_city_label, "city_label",
                  N_("City Label"),
                  N_("This font is used to display the city labels on city "
                     "dialogs."),
                  COC_FONT, GUI_GTK2,
                  "Monospace 8", font_changed_callback),
  GEN_FONT_OPTION(gui_gtk2_font_notify_label, "notify_label",
                  N_("Notify Label"),
                  N_("This font is used to display server reports such "
                     "as the demographic report or historian publications."),
                  COC_FONT, GUI_GTK2,
                  "Monospace Bold 9", font_changed_callback),
  GEN_FONT_OPTION(gui_gtk2_font_spaceship_label, "spaceship_label",
                  N_("Spaceship Label"),
                  N_("This font is used to display the spaceship widgets."),
                  COC_FONT, GUI_GTK2,
                  "Monospace 8", font_changed_callback),
  GEN_FONT_OPTION(gui_gtk2_font_help_label, "help_label",
                  N_("Help Label"),
                  N_("This font is used to display the help headers in the "
                     "help window."),
                  COC_FONT, GUI_GTK2,
                  "Sans Bold 10", font_changed_callback),
  GEN_FONT_OPTION(gui_gtk2_font_help_link, "help_link",
                  N_("Help Link"),
                  N_("This font is used to display the help links in the "
                     "help window."),
                  COC_FONT, GUI_GTK2,
                  "Sans 9", font_changed_callback),
  GEN_FONT_OPTION(gui_gtk2_font_help_text, "help_text",
                  N_("Help Text"),
                  N_("This font is used to display the help body text in "
                     "the help window."),
                  COC_FONT, GUI_GTK2,
                  "Monospace 8", font_changed_callback),
  GEN_FONT_OPTION(gui_gtk2_font_chatline, "chatline",
                  N_("Chatline Area"),
                  N_("This font is used to display the text in the "
                     "chatline area."),
                  COC_FONT, GUI_GTK2,
                  "Monospace 8", font_changed_callback),
  GEN_FONT_OPTION(gui_gtk2_font_beta_label, "beta_label",
                  N_("Beta Label"),
                  N_("This font is used to display the beta label."),
                  COC_FONT, GUI_GTK2,
                  "Sans Italic 10", font_changed_callback),
  GEN_FONT_OPTION(gui_gtk2_font_small, "small_font",
                  N_("Small Font"),
                  N_("This font is used for any small font request.  For "
                     "example, it is used for display the building lists "
                     "in the city dialog, the economic report or the unit "
                     "report."),
                  COC_FONT, GUI_GTK2,
                  "Sans 9", NULL),
  GEN_FONT_OPTION(gui_gtk2_font_comment_label, "comment_label",
                  N_("Comment Label"),
                  N_("This font is used to display comment labels, such as "
                     "in the governor page of the city dialogs."),
                  COC_FONT, GUI_GTK2,
                  "Sans Italic 9", font_changed_callback),
  GEN_FONT_OPTION(gui_gtk2_font_city_names, "city_names",
                  N_("City Names"),
                  N_("This font is used to the display the city names "
                     "on the map."),
                  COC_FONT, GUI_GTK2,
                  "Sans Bold 10", NULL),
  GEN_FONT_OPTION(gui_gtk2_font_city_productions, "city_productions",
                  N_("City Productions"),
                  N_("This font is used to the display the city production "
                     "names on the map."),
                  COC_FONT, GUI_GTK2,
                  "Serif 10", NULL),
  GEN_FONT_OPTION(gui_gtk2_font_reqtree_text, "reqtree_text",
                  N_("Requirement Tree"),
                  N_("This font is used to the display the requirement tree "
                     "in the science report."),
                  COC_FONT, GUI_GTK2,
                  "Serif 10", NULL),

  /* gui-sdl client specific options. */
  GEN_BOOL_OPTION(gui_sdl_fullscreen, N_("Full Screen"), 
                  N_("If this option is set the client will use the "
                     "whole screen area for drawing"),
                  COC_INTERFACE, GUI_SDL, FALSE, NULL),
  GEN_INT_OPTION(gui_sdl_screen_width, N_("Screen width"),
                 N_("This option saves the width of the selected screen "
                    "resolution"),
                 COC_INTERFACE, GUI_SDL, 640, 320, 3200, NULL),
  GEN_INT_OPTION(gui_sdl_screen_height, N_("Screen height"),
                 N_("This option saves the height of the selected screen "
                    "resolution"),
                 COC_INTERFACE, GUI_SDL, 480, 240, 2400, NULL),

  /* gui-win32 client specific options. */
  GEN_BOOL_OPTION(gui_win32_better_fog,
                  N_("Better fog-of-war drawing"),
                  N_("If this is enabled then a better method is used for "
                     "drawing fog-of-war.  It is not any slower but will "
                     "consume about twice as much memory."),
                  COC_GRAPHICS, GUI_WIN32, TRUE, mapview_redraw_callback),
  GEN_BOOL_OPTION(gui_win32_enable_alpha,
                  N_("Enable alpha blending"),
                  N_("If this is enabled, then alpha blending will be "
                     "used in rendering, instead of an ordered dither.  "
                     "If there is no hardware support for alpha "
                     "blending, this is much slower."),
                  COC_GRAPHICS, GUI_WIN32, TRUE, mapview_redraw_callback)
};
static const int client_options_num = ARRAY_SIZE(client_options);

/* Iteration loop, including invalid options for the current gui type. */
#define client_options_iterate_all(poption)                                 \
{                                                                           \
  const struct client_option *const poption##_max =                         \
      client_options + client_options_num;                                  \
  struct client_option *client_##poption = client_options;                  \
  struct option *poption;                                                   \
  for (; client_##poption < poption##_max; client_##poption++) {            \
    poption = OPTION(client_##poption);

#define client_options_iterate_all_end                                      \
  }                                                                         \
}


/****************************************************************************
  Returns the number of the option.
****************************************************************************/
int option_number(const struct option *poption)
{
  fc_assert_ret_val(NULL != poption, -1);

  return poption->common_vtable->number(poption);
}

/****************************************************************************
  Returns the name of the option.
****************************************************************************/
const char *option_name(const struct option *poption)
{
  fc_assert_ret_val(NULL != poption, NULL);

  return poption->common_vtable->name(poption);
}

/****************************************************************************
  Returns the description (translated) of the option.
****************************************************************************/
const char *option_description(const struct option *poption)
{
  fc_assert_ret_val(NULL != poption, NULL);

  return poption->common_vtable->description(poption);
}

/****************************************************************************
  Returns the help text (translated) of the option.
****************************************************************************/
const char *option_help_text(const struct option *poption)
{
  fc_assert_ret_val(NULL != poption, NULL);

  return poption->common_vtable->help_text(poption);
}

/****************************************************************************
  Returns the type of the option.
****************************************************************************/
enum option_type option_type(const struct option *poption)
{
  fc_assert_ret_val(NULL != poption, -1);

  return poption->type;
}

/****************************************************************************
  Returns the category of the option.
****************************************************************************/
int option_category(const struct option *poption)
{
  fc_assert_ret_val(NULL != poption, -1);

  return poption->common_vtable->category(poption);
}

/****************************************************************************
  Returns the next option or NULL if this is the last.
****************************************************************************/
struct option *option_next(const struct option *poption)
{
  fc_assert_ret_val(NULL != poption, NULL);

  return poption->common_vtable->next(poption);
}

/****************************************************************************
  Set the option to its default value.  Returns TRUE if the option changed.
****************************************************************************/
bool option_reset(struct option *poption)
{
  fc_assert_ret_val(NULL != poption, FALSE);

  switch (option_type(poption)) {
  case OT_BOOLEAN:
    return option_bool_set(poption, option_bool_def(poption));
  case OT_INTEGER:
    return option_int_set(poption, option_int_def(poption));
  case OT_STRING:
    return option_str_set(poption, option_str_def(poption));
  case OT_FONT:
    return option_font_set(poption, option_font_def(poption));
  }
  return FALSE;
}

/****************************************************************************
  Set the function to call every time this option changes.  Can be NULL.
****************************************************************************/
void option_set_changed_callback(struct option *poption,
                                 void (*callback) (struct option *))
{
  fc_assert_ret(NULL != poption);

  poption->changed_callback = callback;
}

/****************************************************************************
  Force to use the option changed callback.
****************************************************************************/
void option_changed(struct option *poption)
{
  fc_assert_ret(NULL != poption);

  /* Prevent to use non-initialized datas. */
  if (options_fully_initialized && poption->changed_callback) {
    poption->changed_callback(poption);
  }
}

/****************************************************************************
  Set the gui data for this option.
****************************************************************************/
void option_set_gui_data(struct option *poption, void *data)
{
  fc_assert_ret(NULL != poption);

  poption->gui_data = data;
}

/****************************************************************************
  Returns the gui data of this option.
****************************************************************************/
void *option_get_gui_data(const struct option *poption)
{
  fc_assert_ret_val(NULL != poption, NULL);

  return poption->gui_data;
}

/****************************************************************************
  Returns the current value of this boolean option.
****************************************************************************/
bool option_bool_get(const struct option *poption)
{
  fc_assert_ret_val(NULL != poption, FALSE);
  fc_assert_ret_val(OT_BOOLEAN == poption->type, FALSE);

  return poption->bool_vtable->get(poption);
}

/****************************************************************************
  Returns the default value of this boolean option.
****************************************************************************/
bool option_bool_def(const struct option *poption)
{
  fc_assert_ret_val(NULL != poption, FALSE);
  fc_assert_ret_val(OT_BOOLEAN == poption->type, FALSE);

  return poption->bool_vtable->def(poption);
}

/****************************************************************************
  Sets the value of this boolean option. Returns TRUE if the value changed.
****************************************************************************/
bool option_bool_set(struct option *poption, bool val)
{
  fc_assert_ret_val(NULL != poption, FALSE);
  fc_assert_ret_val(OT_BOOLEAN == poption->type, FALSE);

  if (poption->bool_vtable->set(poption, val)) {
    option_changed(poption);
    return TRUE;
  }
  return FALSE;
}

/****************************************************************************
  Returns the current value of this integer option.
****************************************************************************/
int option_int_get(const struct option *poption)
{
  fc_assert_ret_val(NULL != poption, 0);
  fc_assert_ret_val(OT_INTEGER == poption->type, 0);

  return poption->int_vtable->get(poption);
}

/****************************************************************************
  Returns the default value of this integer option.
****************************************************************************/
int option_int_def(const struct option *poption)
{
  fc_assert_ret_val(NULL != poption, 0);
  fc_assert_ret_val(OT_INTEGER == poption->type, 0);

  return poption->int_vtable->def(poption);
}

/****************************************************************************
  Returns the minimal value of this integer option.
****************************************************************************/
int option_int_min(const struct option *poption)
{
  fc_assert_ret_val(NULL != poption, 0);
  fc_assert_ret_val(OT_INTEGER == poption->type, 0);

  return poption->int_vtable->min(poption);
}

/****************************************************************************
  Returns the maximal value of this integer option.
****************************************************************************/
int option_int_max(const struct option *poption)
{
  fc_assert_ret_val(NULL != poption, 0);
  fc_assert_ret_val(OT_INTEGER == poption->type, 0);

  return poption->int_vtable->max(poption);
}

/****************************************************************************
  Sets the value of this integer option. Returns TRUE if the value changed.
****************************************************************************/
bool option_int_set(struct option *poption, int val)
{
  fc_assert_ret_val(NULL != poption, FALSE);
  fc_assert_ret_val(OT_INTEGER == poption->type, FALSE);

  if (poption->int_vtable->set(poption, val)) {
    option_changed(poption);
    return TRUE;
  }
  return FALSE;
}

/****************************************************************************
  Returns the current value of this string option.
****************************************************************************/
const char *option_str_get(const struct option *poption)
{
  fc_assert_ret_val(NULL != poption, NULL);
  fc_assert_ret_val(OT_STRING == poption->type, NULL);

  return poption->str_vtable->get(poption);
}

/****************************************************************************
  Returns the default value of this string option.
****************************************************************************/
const char *option_str_def(const struct option *poption)
{
  fc_assert_ret_val(NULL != poption, NULL);
  fc_assert_ret_val(OT_STRING == poption->type, NULL);

  return poption->str_vtable->def(poption);
}

/****************************************************************************
  Returns the possible string values of this string option.
****************************************************************************/
const struct strvec *option_str_values(const struct option *poption)
{
  fc_assert_ret_val(NULL != poption, NULL);
  fc_assert_ret_val(OT_STRING == poption->type, NULL);

  return poption->str_vtable->values(poption);
}

/****************************************************************************
  Sets the value of this string option. Returns TRUE if the value changed.
****************************************************************************/
bool option_str_set(struct option *poption, const char *str)
{
  fc_assert_ret_val(NULL != poption, FALSE);
  fc_assert_ret_val(OT_STRING == poption->type, FALSE);
  fc_assert_ret_val(NULL != str, FALSE);

  if (poption->str_vtable->set(poption, str)) {
    option_changed(poption);
    return TRUE;
  }
  return FALSE;
}

/****************************************************************************
  Returns the current value of this string option.
****************************************************************************/
const char *option_font_get(const struct option *poption)
{
  fc_assert_ret_val(NULL != poption, NULL);
  fc_assert_ret_val(OT_FONT == poption->type, NULL);

  return poption->font_vtable->get(poption);
}

/****************************************************************************
  Returns the default value of this string option.
****************************************************************************/
const char *option_font_def(const struct option *poption)
{
  fc_assert_ret_val(NULL != poption, NULL);
  fc_assert_ret_val(OT_FONT == poption->type, NULL);

  return poption->font_vtable->def(poption);
}

/****************************************************************************
  Returns the target style name of this font.
****************************************************************************/
const char *option_font_target(const struct option *poption)
{
  fc_assert_ret_val(NULL != poption, NULL);
  fc_assert_ret_val(OT_FONT == poption->type, NULL);

  return poption->font_vtable->target(poption);
}

/****************************************************************************
  Sets the value of this string option. Returns TRUE if the value changed.
****************************************************************************/
bool option_font_set(struct option *poption, const char *font)
{
  fc_assert_ret_val(NULL != poption, FALSE);
  fc_assert_ret_val(OT_FONT == poption->type, FALSE);
  fc_assert_ret_val(NULL != font, FALSE);

  if (poption->font_vtable->set(poption, font)) {
    option_changed(poption);
    return TRUE;
  }
  return FALSE;
}


/****************************************************************************
  Returns the option corresponding to this id.
****************************************************************************/
struct option *client_option_by_number(int id)
{
  fc_assert_ret_val(0 <= id && id < client_options_num, NULL);

  return OPTION(client_options + id);
}

/***************************************************************************
  Returns the option corresponding to this name.
****************************************************************************/
struct option *client_option_by_name(const char *name)
{
  client_options_iterate(poption) {
    if (0 == strcmp(option_name(poption), name)) {
      return poption;
    }
  } client_options_iterate_end;
  return NULL;
}

/****************************************************************************
  Returns the next valid option pointer for the current gui type.
****************************************************************************/
static struct client_option *
    client_option_next_valid(struct client_option *poption)
{
  const struct client_option *const max = 
    client_options + client_options_num;
  const enum gui_type our_type = get_gui_type();

  while (poption < max
         && poption->specific != GUI_LAST
         && poption->specific != our_type) {
    poption++;
  }

  return (poption < max ? poption : NULL);
}

/****************************************************************************
  Returns the first valid option pointer for the current gui type.
****************************************************************************/
struct option *client_option_first(void)
{
  return OPTION(client_option_next_valid(client_options));
}

/***************************************************************************
  Returns the number of this client option.
****************************************************************************/
static int client_option_number(const struct option *poption)
{
  return CLIENT_OPTION(poption) - client_options;
}

/****************************************************************************
  Returns the name of this client option.
****************************************************************************/
static const char *client_option_name(const struct option *poption)
{
  return CLIENT_OPTION(poption)->name;
}

/****************************************************************************
  Returns the description of this client option.
****************************************************************************/
static const char *client_option_description(const struct option *poption)
{
  return _(CLIENT_OPTION(poption)->description);
}

/****************************************************************************
  Returns the help text for this client option.
****************************************************************************/
static const char *client_option_help_text(const struct option *poption)
{
  return _(CLIENT_OPTION(poption)->help_text);
}

/****************************************************************************
  Returns the category of this client option.
****************************************************************************/
static int client_option_category(const struct option *poption)
{
  return CLIENT_OPTION(poption)->category;
}

/****************************************************************************
  Returns the next valid option pointer for the current gui type.
****************************************************************************/
static struct option *client_option_next(const struct option *poption)
{
  return OPTION(client_option_next_valid(CLIENT_OPTION(poption) + 1));
}

/****************************************************************************
  Returns the value of this client option of type OT_BOOLEAN.
****************************************************************************/
static bool client_option_bool_get(const struct option *poption)
{
  return *(CLIENT_OPTION(poption)->boolean.pvalue);
}

/****************************************************************************
  Returns the default value of this client option of type OT_BOOLEAN.
****************************************************************************/
static bool client_option_bool_def(const struct option *poption)
{
  return CLIENT_OPTION(poption)->boolean.def;
}

/****************************************************************************
  Set the value of this client option of type OT_BOOLEAN.  Returns TRUE if
  the value changed.
****************************************************************************/
static bool client_option_bool_set(struct option *poption, bool val)
{
  struct client_option *pcoption = CLIENT_OPTION(poption);

  if (*pcoption->boolean.pvalue == val) {
    return FALSE;
  }

  *pcoption->boolean.pvalue = val;
  return TRUE;
}

/****************************************************************************
  Returns the value of this client option of type OT_INTEGER.
****************************************************************************/
static int client_option_int_get(const struct option *poption)
{
  return *(CLIENT_OPTION(poption)->integer.pvalue);
}

/****************************************************************************
  Returns the default value of this client option of type OT_INTEGER.
****************************************************************************/
static int client_option_int_def(const struct option *poption)
{
  return CLIENT_OPTION(poption)->integer.def;
}

/****************************************************************************
  Returns the minimal value for this client option of type OT_INTEGER.
****************************************************************************/
static int client_option_int_min(const struct option *poption)
{
  return CLIENT_OPTION(poption)->integer.min;
}

/****************************************************************************
  Returns the maximal value for this client option of type OT_INTEGER.
****************************************************************************/
static int client_option_int_max(const struct option *poption)
{
  return CLIENT_OPTION(poption)->integer.max;
}

/****************************************************************************
  Set the value of this client option of type OT_INTEGER.  Returns TRUE if
  the value changed.
****************************************************************************/
static bool client_option_int_set(struct option *poption, int val)
{
  struct client_option *pcoption = CLIENT_OPTION(poption);

  if (val < pcoption->integer.min
      || val > pcoption->integer.max
      || *pcoption->integer.pvalue == val) {
    return FALSE;
  }

  *pcoption->integer.pvalue = val;
  return TRUE;
}

/****************************************************************************
  Returns the value of this client option of type OT_STRING.
****************************************************************************/
static const char *client_option_str_get(const struct option *poption)
{
  return CLIENT_OPTION(poption)->string.pvalue;
}

/****************************************************************************
  Returns the default value of this client option of type OT_STRING.
****************************************************************************/
static const char *client_option_str_def(const struct option *poption)
{
  return CLIENT_OPTION(poption)->string.def;
}

/****************************************************************************
  Returns the possible string values of this client option of type
  OT_STRING.
****************************************************************************/
static const struct strvec *
    client_option_str_values(const struct option *poption)
{
  return (CLIENT_OPTION(poption)->string.val_accessor
          ? CLIENT_OPTION(poption)->string.val_accessor() : NULL);
}

/****************************************************************************
  Set the value of this client option of type OT_STRING.  Returns TRUE if
  the value changed.
****************************************************************************/
static bool client_option_str_set(struct option *poption, const char *str)
{
  struct client_option *pcoption = CLIENT_OPTION(poption);

  if (strlen(str) >= pcoption->string.size
      || 0 == strcmp(pcoption->string.pvalue, str)) {
    return FALSE;
  }

  mystrlcpy(pcoption->string.pvalue, str, pcoption->string.size);
  return TRUE;
}

/****************************************************************************
  Returns the value of this client option of type OT_FONT.
****************************************************************************/
static const char *client_option_font_get(const struct option *poption)
{
  return CLIENT_OPTION(poption)->font.pvalue;
}

/****************************************************************************
  Returns the default value of this client option of type OT_FONT.
****************************************************************************/
static const char *client_option_font_def(const struct option *poption)
{
  return CLIENT_OPTION(poption)->font.def;
}

/****************************************************************************
  Returns the target style name of this client option of type OT_FONT.
****************************************************************************/
static const char *client_option_font_target(const struct option *poption)
{
  return CLIENT_OPTION(poption)->font.target;
}

/****************************************************************************
  Set the value of this client option of type OT_FONT.  Returns TRUE if
  the value changed.
****************************************************************************/
static bool client_option_font_set(struct option *poption, const char *font)
{
  struct client_option *pcoption = CLIENT_OPTION(poption);

  if (strlen(font) >= pcoption->font.size
      || 0 == strcmp(pcoption->font.pvalue, font)) {
    return FALSE;
  }

  mystrlcpy(pcoption->font.pvalue, font, pcoption->font.size);
  return TRUE;
}

/****************************************************************************
  Load the option from a file.  Returns TRUE if the option changed.
****************************************************************************/
static bool client_option_load(struct option *poption,
                               struct section_file *sf)
{
  fc_assert_ret_val(NULL != poption, FALSE);
  fc_assert_ret_val(NULL != sf, FALSE);

  switch (option_type(poption)) {
  case OT_BOOLEAN:
    {
      bool value;

      return (secfile_lookup_bool(sf, &value, "client.%s",
                                  option_name(poption))
              && option_bool_set(poption, value));
    }
  case OT_INTEGER:
    {
      int value;

      return (secfile_lookup_int(sf, &value, "client.%s",
                                 option_name(poption))
              && option_int_set(poption, value));
    }
  case OT_STRING:
    {
      const char *string;

      return ((string = secfile_lookup_str(sf, "client.%s",
                                           option_name(poption)))
              && option_str_set(poption, string));
    }
  case OT_FONT:
    {
      const char *string;

      return ((string = secfile_lookup_str(sf, "client.%s",
                                           option_name(poption)))
              && option_font_set(poption, string));
    }
  }
  return FALSE;
}

/****************************************************************************
  Returns the number of client option categories.
****************************************************************************/
int client_option_category_number(void)
{
  return COC_MAX;
}

/****************************************************************************
  Returns the name (translated) of the option class.
****************************************************************************/
const char *client_option_category_name(int category)
{
  switch (category) {
  case COC_GRAPHICS:
    return _("Graphics");
  case COC_OVERVIEW:
    return _("Overview");
  case COC_SOUND:
    return _("Sound");
  case COC_INTERFACE:
    return _("Interface");
  case COC_NETWORK:
    return _("Network");
  case COC_FONT:
    return _("Font");
  case COC_MAX:
    break;
  }

  log_error("%s: invalid option category number %d.",
            __FUNCTION__, category);
  return NULL;
}


/** Message Options: **/

int messages_where[E_LAST];


/****************************************************************
  These could be a static table initialisation, except
  its easier to do it this way.
*****************************************************************/
static void message_options_init(void)
{
  int none[] = {
    E_IMP_BUY, E_IMP_SOLD, E_UNIT_BUY,
    E_UNIT_LOST_ATT, E_UNIT_WIN_ATT, E_GAME_START,
    E_NATION_SELECTED, E_CITY_BUILD, E_NEXT_YEAR,
    E_CITY_PRODUCTION_CHANGED,
    E_CITY_MAY_SOON_GROW, E_WORKLIST, E_AI_DEBUG
  };
  int out_only[] = {
    E_CHAT_MSG, E_CHAT_ERROR, E_CONNECTION, E_LOG_ERROR, E_SETTING,
    E_VOTE_NEW, E_VOTE_RESOLVED, E_VOTE_ABORTED
  };
  int all[] = {
    E_LOG_FATAL, E_SCRIPT
  };
  int i;

  for (i = 0; i < E_LAST; i++) {
    messages_where[i] = MW_MESSAGES;
  }
  for (i = 0; i < ARRAY_SIZE(none); i++) {
    messages_where[none[i]] = 0;
  }
  for (i = 0; i < ARRAY_SIZE(out_only); i++) {
    messages_where[out_only[i]] = MW_OUTPUT;
  }
  for (i = 0; i < ARRAY_SIZE(all); i++) {
    messages_where[all[i]] = MW_MESSAGES | MW_POPUP;
  }

  events_init();
}

/****************************************************************
... 
*****************************************************************/
static void message_options_free(void)
{
  events_free();
}

/****************************************************************
  Load the message options; use the function defined by
  specnum.h (see also events.h).
*****************************************************************/
static void message_options_load(struct section_file *file,
                                 const char *prefix)
{
  enum event_type event;
  int i, num_events;
  const char *p;

  if (!secfile_lookup_int(file, &num_events, "messages.count")) {
    /* version < 2.2 */
    /* Order of the events in 2.1. */
    const enum event_type old_events[] = {
      E_CITY_CANTBUILD, E_CITY_LOST, E_CITY_LOVE, E_CITY_DISORDER,
      E_CITY_FAMINE, E_CITY_FAMINE_FEARED, E_CITY_GROWTH,
      E_CITY_MAY_SOON_GROW, E_CITY_AQUEDUCT, E_CITY_AQ_BUILDING,
      E_CITY_NORMAL, E_CITY_NUKED, E_CITY_CMA_RELEASE, E_CITY_GRAN_THROTTLE,
      E_CITY_TRANSFER, E_CITY_BUILD, E_CITY_PRODUCTION_CHANGED,
      E_WORKLIST, E_UPRISING, E_CIVIL_WAR, E_ANARCHY, E_FIRST_CONTACT,
      E_NEW_GOVERNMENT, E_LOW_ON_FUNDS, E_POLLUTION, E_REVOLT_DONE,
      E_REVOLT_START, E_SPACESHIP, E_MY_DIPLOMAT_BRIBE,
      E_DIPLOMATIC_INCIDENT, E_MY_DIPLOMAT_ESCAPE, E_MY_DIPLOMAT_EMBASSY,
      E_MY_DIPLOMAT_FAILED, E_MY_DIPLOMAT_INCITE, E_MY_DIPLOMAT_POISON,
      E_MY_DIPLOMAT_SABOTAGE, E_MY_DIPLOMAT_THEFT, E_ENEMY_DIPLOMAT_BRIBE,
      E_ENEMY_DIPLOMAT_EMBASSY, E_ENEMY_DIPLOMAT_FAILED,
      E_ENEMY_DIPLOMAT_INCITE, E_ENEMY_DIPLOMAT_POISON,
      E_ENEMY_DIPLOMAT_SABOTAGE, E_ENEMY_DIPLOMAT_THEFT,
      E_CARAVAN_ACTION, E_SCRIPT, E_BROADCAST_REPORT, E_GAME_END,
      E_GAME_START, E_NATION_SELECTED, E_DESTROYED, E_REPORT, E_TURN_BELL,
      E_NEXT_YEAR, E_GLOBAL_ECO, E_NUKE, E_HUT_BARB, E_HUT_CITY, E_HUT_GOLD,
      E_HUT_BARB_KILLED, E_HUT_MERC, E_HUT_SETTLER, E_HUT_TECH,
      E_HUT_BARB_CITY_NEAR, E_IMP_BUY, E_IMP_BUILD, E_IMP_AUCTIONED,
      E_IMP_AUTO, E_IMP_SOLD, E_TECH_GAIN, E_TECH_LEARNED, E_TREATY_ALLIANCE,
      E_TREATY_BROKEN, E_TREATY_CEASEFIRE, E_TREATY_PEACE,
      E_TREATY_SHARED_VISION, E_UNIT_LOST_ATT, E_UNIT_WIN_ATT, E_UNIT_BUY,
      E_UNIT_BUILT, E_UNIT_LOST_DEF, E_UNIT_WIN, E_UNIT_BECAME_VET,
      E_UNIT_UPGRADED, E_UNIT_RELOCATED, E_UNIT_ORDERS, E_WONDER_BUILD,
      E_WONDER_OBSOLETE, E_WONDER_STARTED, E_WONDER_STOPPED,
      E_WONDER_WILL_BE_BUILT, E_DIPLOMACY, E_TREATY_EMBASSY,
      E_BAD_COMMAND, E_SETTING, E_CHAT_MSG, E_MESSAGE_WALL, E_CHAT_ERROR,
      E_CONNECTION, E_AI_DEBUG
    };
    const size_t old_events_num = ARRAY_SIZE(old_events);

    for (i = 0; i < old_events_num; i++) {
      messages_where[old_events[i]] =
        secfile_lookup_int_default(file, messages_where[old_events[i]],
                                   "%s.message_where_%02d", prefix, i);
    }
    return;
  }

  for (i = 0; i < num_events; i++) {
    p = secfile_lookup_str(file, "messages.event%d.name", i);
    if (NULL == p) {
      log_error("Corruption in file %s: %s",
                secfile_name(file), secfile_error());
      continue;
    }
    event = event_type_by_name(p, strcmp);
    if (!event_type_is_valid(event)) {
      log_error("Event not supported: %s", p);
      continue;
    }

    /* skip E_LAST */
    if (event == E_LAST) {
      continue;
    }

    if (!secfile_lookup_int(file, &messages_where[event],
                            "messages.event%d.where", i)) {
      log_error("Corruption in file %s: %s",
                secfile_name(file), secfile_error());
    }
  }
}

/****************************************************************
  Save the message options; use the function defined by
  specnum.h (see also events.h).
*****************************************************************/
static void message_options_save(struct section_file *file,
                                 const char *prefix)
{
  enum event_type event;
  int i = 0;

  for (event = event_type_begin(); event != event_type_end();
       event = event_type_next(event)) {
    /* skip E_LAST */
    if (event == E_LAST) {
      continue;
    }

    secfile_insert_str(file, event_type_name(event),
                       "messages.event%d.name", i);
    secfile_insert_int(file, messages_where[i],
                       "messages.event%d.where", i);
    i++;
  }

  secfile_insert_int(file, i, "messages.count");
}


/****************************************************************
 Does heavy lifting for looking up a preset.
*****************************************************************/
static void load_cma_preset(struct section_file *file, int i)
{
  struct cm_parameter parameter;
  const char *name =
    secfile_lookup_str_default(file, "preset",
                               "cma.preset%d.name", i);

  output_type_iterate(o) {
    parameter.minimal_surplus[o] =
        secfile_lookup_int_default(file, 0, "cma.preset%d.minsurp%d", i, o);
    parameter.factor[o] =
        secfile_lookup_int_default(file, 0, "cma.preset%d.factor%d", i, o);
  } output_type_iterate_end;
  parameter.require_happy =
      secfile_lookup_bool_default(file, FALSE, "cma.preset%d.reqhappy", i);
  parameter.happy_factor =
      secfile_lookup_int_default(file, 0, "cma.preset%d.happyfactor", i);
  parameter.allow_disorder = FALSE;
  parameter.allow_specialists = TRUE;

  cmafec_preset_add(name, &parameter);
}

/****************************************************************
 Does heavy lifting for inserting a preset.
*****************************************************************/
static void save_cma_preset(struct section_file *file, int i)
{
  const struct cm_parameter *const pparam = cmafec_preset_get_parameter(i);
  char *name = cmafec_preset_get_descr(i);

  secfile_insert_str(file, name, "cma.preset%d.name", i);

  output_type_iterate(o) {
    secfile_insert_int(file, pparam->minimal_surplus[o],
                       "cma.preset%d.minsurp%d", i, o);
    secfile_insert_int(file, pparam->factor[o],
                       "cma.preset%d.factor%d", i, o);
  } output_type_iterate_end;
  secfile_insert_bool(file, pparam->require_happy,
                      "cma.preset%d.reqhappy", i);
  secfile_insert_int(file, pparam->happy_factor,
                     "cma.preset%d.happyfactor", i);
}

/****************************************************************
 Insert all cma presets.
*****************************************************************/
static void save_cma_presets(struct section_file *file)
{
  int i;

  secfile_insert_int_comment(file, cmafec_preset_num(),
                             _("If you add a preset by hand,"
                               " also update \"number_of_presets\""),
                             "cma.number_of_presets");
  for (i = 0; i < cmafec_preset_num(); i++) {
    save_cma_preset(file, i);
  }
}


/* Old rc file name. */
#define OLD_OPTION_FILE_NAME ".civclientrc"
/* New rc file name. */
#define NEW_OPTION_FILE_NAME ".freeciv-client-rc-%d.%d"
#define MAJOR_NEW_OPTION_FILE_NAME MAJOR_VERSION
#if IS_DEVEL_VERSION
#define MINOR_NEW_OPTION_FILE_NAME (MINOR_VERSION + 1)
#else
#define MINOR_NEW_OPTION_FILE_NAME MINOR_VERSION
#endif /* IS_DEVEL_VERSION */
/* The first version the new option name appeared (2.2). */
#define FIRST_MAJOR_NEW_OPTION_FILE_NAME 2
#define FIRST_MINOR_NEW_OPTION_FILE_NAME 2
/****************************************************************
  Returns pointer to static memory containing name of the current
  option file.  Usually used for saving.
  Ie, based on FREECIV_OPT env var, and home dir. (or a
  OPTION_FILE_NAME define defined in config.h)
  Or NULL if problem.
*****************************************************************/
static const char *get_current_option_file_name(void)
{
  static char name_buffer[256];
  const char *name;

  name = getenv("FREECIV_OPT");

  if (name) {
    sz_strlcpy(name_buffer, name);
  } else {
#ifdef OPTION_FILE_NAME
    mystrlcpy(name_buffer, OPTION_FILE_NAME, sizeof(name_buffer));
#else
    name = user_home_dir();
    if (!name) {
      log_error(_("Cannot find your home directory"));
      return NULL;
    }
    my_snprintf(name_buffer, sizeof(name_buffer),
                "%s/" NEW_OPTION_FILE_NAME, name,
                MAJOR_NEW_OPTION_FILE_NAME, MINOR_NEW_OPTION_FILE_NAME);
#endif /* OPTION_FILE_NAME */
  }
  log_verbose("settings file is %s", name_buffer);
  return name_buffer;
}

/****************************************************************
  Check the last option file we saved.  Usually used to load.
  Ie, based on FREECIV_OPT env var, and home dir. (or a
  OPTION_FILE_NAME define defined in config.h)
  Or NULL if not found.
*****************************************************************/
static const char *get_last_option_file_name(void)
{
  static char name_buffer[256];
  const char *name;

  name = getenv("FREECIV_OPT");

  if (name) {
    sz_strlcpy(name_buffer, name);
  } else {
#ifdef OPTION_FILE_NAME
    mystrlcpy(name_buffer, OPTION_FILE_NAME, sizeof(name_buffer));
#else
    int major, minor;
    struct stat buf;

    name = user_home_dir();
    if (!name) {
      log_error(_("Cannot find your home directory"));
      return NULL;
    }
    for (major = MAJOR_NEW_OPTION_FILE_NAME,
         minor = MINOR_NEW_OPTION_FILE_NAME;
         major >= FIRST_MAJOR_NEW_OPTION_FILE_NAME; major--) {
      for (; (major == FIRST_MAJOR_NEW_OPTION_FILE_NAME
              ? minor >= FIRST_MINOR_NEW_OPTION_FILE_NAME 
              : minor >= 0); minor--) {
        my_snprintf(name_buffer, sizeof(name_buffer),
                    "%s/" NEW_OPTION_FILE_NAME, name, major, minor);
        if (0 == fc_stat(name_buffer, &buf)) {
          if (MAJOR_NEW_OPTION_FILE_NAME != major
              || MINOR_NEW_OPTION_FILE_NAME != minor) {
            log_normal(_("Didn't find '%s' option file, "
                         "loading from '%s' instead."),
                       get_current_option_file_name() + strlen(name) + 1,
                       name_buffer + strlen(name) + 1);
          }
          return name_buffer;
        }
      }
      minor = 99;       /* Looks enough big. */
    }
    /* Try with the old one. */
    my_snprintf(name_buffer, sizeof(name_buffer),
                "%s/" OLD_OPTION_FILE_NAME, name);
    if (0 == fc_stat(name_buffer, &buf)) {
      log_normal(_("Didn't find '%s' option file, "
                   "loading from '%s' instead."),
                 get_current_option_file_name() + strlen(name) + 1,
                 OLD_OPTION_FILE_NAME);
      return name_buffer;
    } else {
      return NULL;
    }
#endif /* OPTION_FILE_NAME */
  }
  log_verbose("settings file is %s", name_buffer);
  return name_buffer;
}
#undef OLD_OPTION_FILE_NAME
#undef NEW_OPTION_FILE_NAME
#undef FIRST_MAJOR_NEW_OPTION_FILE_NAME
#undef FIRST_MINOR_NEW_OPTION_FILE_NAME


/**************************************************************************
  Load the server options.
**************************************************************************/
static void settable_options_load(struct section_file *sf)
{
  char buf[64];
  const struct section *psection;
  const struct entry_list *entries;
  const char *string;
  bool bval;
  int ival;

  fc_assert_ret(NULL != settable_options_hash);

  hash_delete_all_entries(settable_options_hash);

  psection = secfile_section_by_name(sf, "server");
  if (NULL == psection) {
    /* Does not exist! */
    return;
  }

  entries = section_entries(psection);
  entry_list_iterate(entries, pentry) {
    string = NULL;
    switch (entry_type(pentry)) {
    case ENTRY_BOOL:
      if (entry_bool_get(pentry, &bval)) {
        my_snprintf(buf, sizeof(buf), "%d", bval);
        string = buf;
      }
      break;

    case ENTRY_INT:
      if (entry_int_get(pentry, &ival)) {
        my_snprintf(buf, sizeof(buf), "%d", ival);
        string = buf;
      }
      break;

    case ENTRY_STR:
      (void) entry_str_get(pentry, &string);
      break;
    }

    if (NULL == string) {
      log_error("Entry type variant of \"%s.%s\" is not supported.",
                section_name(psection), entry_name(pentry));
      continue;
    }

    hash_insert(settable_options_hash,
                mystrdup(entry_name(pentry)), mystrdup(string));
  } entry_list_iterate_end;
}

/****************************************************************
  Save the desired server options.
*****************************************************************/
static void settable_options_save(struct section_file *sf)
{
  fc_assert_ret(NULL != settable_options_hash);

  hash_iterate(settable_options_hash, iter) {
    secfile_insert_str(sf, (const char *) hash_iter_get_value(iter),
                       "server.%s", (const char *) hash_iter_get_key(iter));
  } hash_iterate_end;
}

/****************************************************************
  Update the desired settable options hash table from the current
  setting configuration.
*****************************************************************/
void desired_settable_options_update(void)
{
  char val_buf[64], def_buf[64];
  struct options_settable *pset;
  const char *value, *def_val;
  int i;

  fc_assert_ret(NULL != settable_options_hash);

  for (i = 0; i < num_settable_options; i++) {
    pset = settable_options + i;
    if (!pset->is_visible) {
      /* Cannot know the value of this setting in this case,
       * don't overwrite. */
      continue;
    }

    value = NULL;
    def_val = NULL;
    switch (pset->stype) {
    case SSET_BOOL:
    case SSET_INT:
      my_snprintf(val_buf, sizeof(val_buf), "%d", pset->val);
      value = val_buf;
      my_snprintf(def_buf, sizeof(def_buf), "%d", pset->default_val);
      def_val = def_buf;
      break;
    case SSET_STRING:
      value = pset->strval;
      def_val = pset->default_strval;
      break;
    }

    if (NULL == value || NULL == def_val) {
      log_error("Wrong setting type (%d) for '%s'.",
                pset->stype, pset->name);
      continue;
    }

    if (0 == strcmp(value, def_val)) {
      /* Not set, using default... */
      hash_delete_entry(settable_options_hash, pset->name);
    } else {
      /* Really desired. */
      hash_replace(settable_options_hash,
                   mystrdup(pset->name), mystrdup(value));
    }
  }
}

/****************************************************************
  Update a desired settable option in the hash table from a value
  which can be different of the current consiguration.
*****************************************************************/
void desired_settable_option_update(const char *op_name,
                                    const char *op_value,
                                    bool allow_replace)
{
  fc_assert_ret(NULL != settable_options_hash);

  if (allow_replace) {
    hash_delete_entry(settable_options_hash, op_name);
  }
  hash_insert(settable_options_hash, mystrdup(op_name), mystrdup(op_value));
}

/****************************************************************
  Send the desired server options to the server.
*****************************************************************/
void desired_settable_option_send(struct options_settable *pset)
{
  char buf[64];
  const char *desired;
  const char *value;

  fc_assert_ret(NULL != settable_options_hash);

  desired = hash_lookup_data(settable_options_hash, pset->name);
  if (NULL == desired) {
    /* No change explicitly  desired. */
    return;
  }

  value = NULL;
  switch (pset->stype) {
  case SSET_BOOL:
  case SSET_INT:
    my_snprintf(buf, sizeof(buf), "%d", pset->val);
    value = buf;
    break;
  case SSET_STRING:
    value = pset->strval;
    break;
  }

  if (NULL != value) {
    if (0 != strcmp(value, desired)) {
      send_chat_printf("/set %s %s", pset->name, desired);
    }
  } else {
    log_error("Wrong setting type (%d) for '%s'.",
              pset->stype, pset->name);
  }
}


/****************************************************************
  Load the city and player report dialog options.
*****************************************************************/
static void options_dialogs_load(struct section_file *sf)
{
  const struct entry_list *entries;
  const char *prefixes[] = { "player_dlg_", "city_report_", NULL };
  const char **prefix;
  bool visible;

  fc_assert_ret(NULL != dialog_options_hash);

  entries = section_entries(secfile_section_by_name(sf, "client"));

  if (NULL != entries) {
    entry_list_iterate(entries, pentry) {
      for (prefix = prefixes; NULL != *prefix; prefix++) {
        if (0 == strncmp(*prefix, entry_name(pentry), strlen(*prefix))
            && secfile_lookup_bool(sf, &visible, "client.%s",
                                   entry_name(pentry))) {
          hash_replace(dialog_options_hash, mystrdup(entry_name(pentry)),
                       FC_INT_TO_PTR(visible));
          break;
        }
      }
    } entry_list_iterate_end;
  }
}

/****************************************************************
  Save the city and player report dialog options.
*****************************************************************/
static void options_dialogs_save(struct section_file *sf)
{
  fc_assert_ret(NULL != dialog_options_hash);

  options_dialogs_update();
  hash_iterate(dialog_options_hash, iter) {
    secfile_insert_bool(sf, FC_PTR_TO_INT(hash_iter_get_value(iter)),
                        "client.%s", (const char *) hash_iter_get_key(iter));
  } hash_iterate_end;
}

/****************************************************************
  This set the city and player report dialog options to the
  current ones.  It's called when the client goes to
  C_S_DISCONNECTED state.
*****************************************************************/
void options_dialogs_update(void)
{
  char buf[64];
  int i;

  fc_assert_ret(NULL != dialog_options_hash);

  /* Player report dialog options. */
  for (i = 1; i < num_player_dlg_columns; i++) {
    my_snprintf(buf, sizeof(buf), "player_dlg_%s",
                player_dlg_columns[i].tagname);
    hash_replace(dialog_options_hash, mystrdup(buf),
                 FC_INT_TO_PTR(player_dlg_columns[i].show));
  }

  /* City report dialog options. */
  for (i = 0; i < num_city_report_spec(); i++) {
    my_snprintf(buf, sizeof(buf), "city_report_%s",
                city_report_spec_tagname(i));
    hash_replace(dialog_options_hash, mystrdup(buf),
                 FC_INT_TO_PTR(*city_report_spec_show_ptr(i)));
  }
}

/****************************************************************
  This set the city and player report dialog options.  It's called
  when the client goes to C_S_RUNNING state.
*****************************************************************/
void options_dialogs_set(void)
{
  char buf[64];
  const void *data;
  int i;

  fc_assert_ret(NULL != dialog_options_hash);

  /* Player report dialog options. */
  for (i = 1; i < num_player_dlg_columns; i++) {
    my_snprintf(buf, sizeof(buf), "player_dlg_%s",
                player_dlg_columns[i].tagname);
    if (hash_lookup(dialog_options_hash, buf, NULL, &data)) {
      player_dlg_columns[i].show = FC_PTR_TO_INT(data);
    }
  }

  /* City report dialog options. */
  for (i = 0; i < num_city_report_spec(); i++) {
    my_snprintf(buf, sizeof(buf), "city_report_%s",
                city_report_spec_tagname(i));
    if (hash_lookup(dialog_options_hash, buf, NULL, &data)) {
      *city_report_spec_show_ptr(i) = FC_PTR_TO_INT(data);
    }
  }
}


/****************************************************************
  Load from the rc file any options that are not ruleset specific.
  It is called after ui_init(), yet before ui_main().
  Unfortunately, this means that some clients cannot display.
  Instead, use log_*().
*****************************************************************/
void options_load(void)
{
  struct section_file *sf;
  int i, num;
  const char *name;
  const char * const prefix = "client";

  name = get_last_option_file_name();
  if (!name) {
    log_normal(_("Didn't find the option file."));
    options_fully_initialized = TRUE;
    create_default_cma_presets();
    return;
  }
  if (!(sf = secfile_load(name, TRUE))) {
    log_debug("Error loading option file '%s':\n%s", name, secfile_error());
    /* try to create the rc file */
    sf = secfile_new(TRUE);
    secfile_insert_str(sf, VERSION_STRING, "client.version");

    create_default_cma_presets();
    save_cma_presets(sf);

    /* FIXME: need better messages */
    if (!secfile_save(sf, name, 0, FZ_PLAIN)) {
      log_error(_("Save failed, cannot write to file %s"), name);
    } else {
      log_normal(_("Saved settings to file %s"), name);
    }
    secfile_destroy(sf);
    options_fully_initialized = TRUE;
    return;
  }

  /* a "secret" option for the lazy. TODO: make this saveable */
  sz_strlcpy(password,
             secfile_lookup_str_default(sf, "", "%s.password", prefix));

  save_options_on_exit =
    secfile_lookup_bool_default(sf, save_options_on_exit,
                                "%s.save_options_on_exit", prefix);
  fullscreen_mode =
    secfile_lookup_bool_default(sf, fullscreen_mode,
                                "%s.fullscreen_mode", prefix);

  client_options_iterate_all(poption) {
    client_option_load(poption, sf);
  } client_options_iterate_all_end;

  message_options_load(sf, prefix);
  options_dialogs_load(sf);

  /* Load cma presets. If cma.number_of_presets doesn't exist, don't load 
   * any, the order here should be reversed to keep the order the same */
  if (secfile_lookup_int(sf, &num, "cma.number_of_presets")) {
    for (i = num - 1; i >= 0; i--) {
      load_cma_preset(sf, i);
    }
  } else {
    create_default_cma_presets();
  }

  settable_options_load(sf);
  global_worklists_load(sf);

  secfile_destroy(sf);
  options_fully_initialized = TRUE;
}

/**************************************************************************
  Save all options.
**************************************************************************/
void options_save(void)
{
  struct section_file *sf;
  const char *name = get_current_option_file_name();

  if (!name) {
    output_window_append(ftc_client,
                         _("Save failed, cannot find a filename."));
    return;
  }

  sf = secfile_new(TRUE);
  secfile_insert_str(sf, VERSION_STRING, "client.version");

  secfile_insert_bool(sf, save_options_on_exit, "client.save_options_on_exit");
  secfile_insert_bool(sf, fullscreen_mode, "client.fullscreen_mode");

  client_options_iterate_all(poption) {
    switch (option_type(poption)) {
    case OT_BOOLEAN:
      secfile_insert_bool(sf, option_bool_get(poption),
                          "client.%s", option_name(poption));
      break;
    case OT_INTEGER:
      secfile_insert_int(sf, option_int_get(poption),
                         "client.%s", option_name(poption));
      break;
    case OT_STRING:
      secfile_insert_str(sf, option_str_get(poption),
                         "client.%s", option_name(poption));
      break;
    case OT_FONT:
      secfile_insert_str(sf, option_font_get(poption),
                         "client.%s", option_name(poption));
      break;
    }
  } client_options_iterate_all_end;

  message_options_save(sf, "client");
  options_dialogs_save(sf);

  /* server settings */
  save_cma_presets(sf);
  settable_options_save(sf);

  /* insert global worklists */
  global_worklists_save(sf);

  /* save to disk */
  if (!secfile_save(sf, name, 0, FZ_PLAIN)) {
    output_window_printf(ftc_client,
                         _("Save failed, cannot write to file %s"), name);
  } else {
    output_window_printf(ftc_client, _("Saved settings to file %s"), name);
  }
  secfile_destroy(sf);
}


/**************************************************************************
  Initialize the option module.
**************************************************************************/
void options_init(void)
{
  message_options_init();
  gui_options_extra_init();
  global_worklists_init();

  settable_options_hash = hash_new_full(hash_fval_string, hash_fcmp_string,
                                        free, free);
  dialog_options_hash = hash_new_full(hash_fval_string, hash_fcmp_string,
                                      free, NULL);

  client_options_iterate_all(poption) {
    switch (option_type(poption)) {
    case OT_BOOLEAN:
      break;

    case OT_INTEGER:
      if (option_int_def(poption) < option_int_min(poption)
          || option_int_def(poption) > option_int_max(poption)) {
        int new_default = MAX(MIN(option_int_def(poption),
                                  option_int_max(poption)),
                              option_int_min(poption));

        log_error("option %s has default value of %d, which is "
                  "out of its range [%d; %d], changing to %d.",
                  option_name(poption), option_int_def(poption),
                  option_int_min(poption), option_int_max(poption),
                  new_default);
        *((int *) &(CLIENT_OPTION(poption)->integer.def)) = new_default;
      }
      break;

    case OT_STRING:
      if (default_user_name == option_str_get(poption)) {
        /* Hack to get a default value. */
        *((const char **) &(CLIENT_OPTION(poption)->string.def)) =
            mystrdup(default_user_name);
      }

      if (NULL == option_str_def(poption)) {
        const struct strvec *values = option_str_values(poption);

        if (NULL == values || strvec_size(values) == 0) {
          log_error("Invalid NULL default string for option %s.",
                    option_name(poption));
        } else {
          *((const char **) &(CLIENT_OPTION(poption)->string.def)) =
              strvec_get(values, 0);
        }
      }
      break;

    case OT_FONT:
      break;
    }

    /* Set to default. */
    option_reset(poption);
  } client_options_iterate_all_end;
}

/**************************************************************************
  Free the option module.
**************************************************************************/
void options_free(void)
{
  if (NULL != settable_options_hash) {
    hash_free(settable_options_hash);
    settable_options_hash = NULL;
  }

  if (NULL != dialog_options_hash) {
    hash_free(dialog_options_hash);
    dialog_options_hash = NULL;
  }

  message_options_free();
  global_worklists_free();
}


/****************************************************************************
  Callback when a mapview graphics option is changed (redraws the canvas).
****************************************************************************/
static void mapview_redraw_callback(struct option *poption)
{
  update_map_canvas_visible();
}

/****************************************************************************
   Callback when the reqtree  show icons option is changed.
   The tree is recalculated.
****************************************************************************/
static void reqtree_show_icons_callback(struct option *poption)
{
  /* This will close research dialog, when it's open again the techtree will
   * be recalculated */
  popdown_all_game_dialogs();
}

/****************************************************************************
  Callback for when any view option is changed.
****************************************************************************/
static void view_option_changed_callback(struct option *poption)
{
  menus_init();
  update_map_canvas_visible();
}

/****************************************************************************
  Callback for when any voeinfo bar option is changed.
****************************************************************************/
static void voteinfo_bar_callback(struct option *poption)
{
  voteinfo_gui_update();
}

/****************************************************************************
  Callback for font options.
****************************************************************************/
static void font_changed_callback(struct option *poption)
{
  fc_assert_ret(OT_FONT == option_type(OPTION(poption)));
  gui_update_font(option_font_target(poption), option_font_get(poption));
}
