/*
 * libj4status-plugin - Library to implement a j4status plugin
 *
 * Copyright © 2012-2013 Quentin "Sardem FF7" Glidic
 *
 * This file is part of j4status.
 *
 * j4status is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * j4status is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with j4status. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef __J4STATUS_J4STATUS_PLUGIN_PRIVATE_H__
#define __J4STATUS_J4STATUS_PLUGIN_PRIVATE_H__

struct _J4statusSection {
    const gchar *name;
    gchar *instance;
    gchar *label;
    gchar *value;
    J4statusState state;
    gpointer user_data;
    gboolean dirty;
    gchar *cache;
};


typedef void(*J4statusCoreFunc)(J4statusCoreContext *context);

struct _J4statusCoreInterface {
    J4statusCoreFunc trigger_display;
};


struct _J4statusOutputPluginInterface {
    J4statusPluginInitFunc   init;
    J4statusPluginSimpleFunc uninit;

    J4statusPluginPrintFunc print;
};

struct _J4statusInputPluginInterface {
    J4statusPluginInitFunc   init;
    J4statusPluginSimpleFunc uninit;

    J4statusPluginGetSectionsFunc get_sections;

    J4statusPluginSimpleFunc start;
    J4statusPluginSimpleFunc stop;
};

#endif /* __J4STATUS_J4STATUS_PLUGIN_PRIVATE_H__ */
