/*
 * j4status - Status line generator
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

#include <stdlib.h>

#include <glib.h>
#include <sensors/sensors.h>

#include <j4status-plugin.h>
#include <libj4status-config.h>

struct _J4statusPluginContext {
    J4statusCoreContext *core;
    J4statusCoreInterface *core_interface;
    GList *sections;
    struct {
        gboolean show_details;
    } config;
    gboolean started;
};

typedef struct {
    J4statusPluginContext *context;
    const sensors_chip_name *chip;
    const sensors_feature *feature;
    const sensors_subfeature *input;
    const sensors_subfeature *max;
    const sensors_subfeature *crit;
} J4statusSensorsFeature;

static gboolean
_j4status_sensors_feature_temp_update(gpointer user_data)
{
    J4statusSection *section = user_data;
    J4statusSensorsFeature *feature = j4status_section_get_user_data(section);
    J4statusPluginContext *context = feature->context;

    double curr;
    sensors_get_value(feature->chip, feature->input->number, &curr);

    double high = -1;
    if ( feature->max != NULL )
        sensors_get_value(feature->chip, feature->max->number, &high);

    double crit = -1;
    if ( feature->crit != NULL )
        sensors_get_value(feature->chip, feature->crit->number, &crit);

    gboolean update = context->started;

    if ( ( crit > 0 ) && ( curr > crit ) )
    {
        j4status_section_set_state(section, J4STATUS_STATE_URGENT);
        update = TRUE;
    }
    else if ( ( high > 0 ) && ( curr > high ) )
        j4status_section_set_state(section, J4STATUS_STATE_BAD);
    else
        j4status_section_set_state(section, J4STATUS_STATE_GOOD);

    if ( ! update )
        return TRUE;

    if ( ! context->config.show_details )
        high = crit = -1;

    gchar *value;
    if ( ( high > 0 ) && ( crit > 0 ) )
        value = g_strdup_printf("%+.1f°C (high = %+.1f°C, crit = %+.1f°C)", curr, high, crit);
    else if ( high > 0 )
        value = g_strdup_printf("%+.1f°C (high = %+.1f°C)", curr, high);
    else if ( crit > 0 )
        value = g_strdup_printf("%+.1f°C (crit = %+.1f°C)", curr, crit);
    else
        value = g_strdup_printf("%+.1f°C", curr);
    j4status_section_set_value(section, value);

    libj4status_core_trigger_display(context->core, context->core_interface);

    return TRUE;
}

static void
_j4status_sensors_add_feature_temp(J4statusPluginContext *context, const sensors_chip_name *chip, const sensors_feature *feature)
{
    const sensors_subfeature *input;

    input = sensors_get_subfeature(chip, feature, SENSORS_SUBFEATURE_TEMP_INPUT);
    if ( input == NULL )
    {
        char n[256];
        sensors_snprintf_chip_name(n, sizeof(n), chip);
        g_warning("No temperature input on chip '%s', skipping", n);
        return;
    }

    J4statusSensorsFeature *sensor_feature;
    sensor_feature = g_new0(J4statusSensorsFeature, 1);
    sensor_feature->context = context;
    sensor_feature->chip = chip;
    sensor_feature->feature = feature;
    sensor_feature->input = input;
    sensor_feature->max = sensors_get_subfeature(chip, feature, SENSORS_SUBFEATURE_TEMP_MAX);
    sensor_feature->crit = sensors_get_subfeature(chip, feature, SENSORS_SUBFEATURE_TEMP_CRIT);

    J4statusSection *section;
    section = j4status_section_new("sensors", sensor_feature);

    char *label;
    label = sensors_get_label(chip, feature);
    j4status_section_set_label(section, label);
    free(label);

    g_timeout_add_seconds(2, _j4status_sensors_feature_temp_update, section);

    context->sections = g_list_prepend(context->sections, section);
}

static void
_j4status_sensors_add_sensors(J4statusPluginContext *context, const sensors_chip_name *match)
{
    const sensors_chip_name *chip;
    const sensors_feature *feature;
    int chip_nr = 0;
    while ( ( chip = sensors_get_detected_chips(match, &chip_nr) ) != NULL )
    {
        int feature_nr = 0;
        while ( ( feature = sensors_get_features(chip, &feature_nr) ) != NULL )
        {
            switch (feature->type) {
            case SENSORS_FEATURE_TEMP:
                _j4status_sensors_add_feature_temp(context, chip, feature);
            break;
            default:
                continue;
            }
        }
    }
}

static J4statusPluginContext *
_j4status_sensors_init(J4statusCoreContext *core, J4statusCoreInterface *core_interface)
{
    gchar **sensors = NULL;
    gboolean show_details = FALSE;

    GKeyFile *key_file;
    key_file = libj4status_config_get_key_file("Sensors");
    if ( key_file != NULL )
    {
        sensors = g_key_file_get_string_list(key_file, "Sensors", "Sensors", NULL, NULL);
        show_details = g_key_file_get_boolean(key_file, "Sensors", "ShowDetails", NULL);
        g_key_file_free(key_file);
    }

    if ( sensors_init(NULL) != 0 )
    {
        g_strfreev(sensors);
        return NULL;
    }

    J4statusPluginContext *context;
    context = g_new0(J4statusPluginContext, 1);
    context->core = core;
    context->core_interface = core_interface;

    context->config.show_details = show_details;


    if ( sensors == NULL )
        _j4status_sensors_add_sensors(context, NULL);
    else
    {
        sensors_chip_name chip;
        gchar **sensor;
        for ( sensor = sensors ; *sensor != NULL ; ++sensor )
        {
            if ( sensors_parse_chip_name(*sensor, &chip) != 0 )
                continue;
            _j4status_sensors_add_sensors(context, &chip);
            sensors_free_chip_name(&chip);
        }
    }
    g_strfreev(sensors);

    context->sections = g_list_reverse(context->sections);
    return context;
}

static void
_j4status_sensors_section_free(gpointer data)
{
    J4statusSection *section = data;
    J4statusSensorsFeature *feature = j4status_section_get_user_data(section);

    g_free(feature);

    j4status_section_free(section);
}

static void
_j4status_sensors_uninit(J4statusPluginContext *context)
{
    if ( context == NULL )
        return;

    g_list_free_full(context->sections, _j4status_sensors_section_free);

    g_free(context);

    sensors_cleanup();
}

static GList **
_j4status_sensors_get_sections(J4statusPluginContext *context)
{
    if ( context == NULL )
        return NULL;

    return &context->sections;
}

static void
_j4status_sensors_start(J4statusPluginContext *context)
{
    if ( context == NULL )
        return;

    context->started = TRUE;

    GList *section;
    for ( section = context->sections ; section != NULL ; section = g_list_next(section) )
        _j4status_sensors_feature_temp_update(section->data);
}

static void
_j4status_sensors_stop(J4statusPluginContext *context)
{
    if ( context == NULL )
        return;

    context->started = FALSE;
}

void
j4status_input_plugin(J4statusInputPluginInterface *interface)
{
    libj4status_input_plugin_interface_add_init_callback(interface, _j4status_sensors_init);
    libj4status_input_plugin_interface_add_uninit_callback(interface, _j4status_sensors_uninit);

    libj4status_input_plugin_interface_add_get_sections_callback(interface, _j4status_sensors_get_sections);

    libj4status_input_plugin_interface_add_start_callback(interface, _j4status_sensors_start);
    libj4status_input_plugin_interface_add_stop_callback(interface, _j4status_sensors_stop);
}
