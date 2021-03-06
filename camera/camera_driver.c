/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
 * 
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include <string.h>
#include <string/string_list.h>
#include "camera_driver.h"
#include "../driver.h"
#include "../general.h"

static const camera_driver_t *camera_drivers[] = {
#ifdef HAVE_V4L2
   &camera_v4l2,
#endif
#ifdef EMSCRIPTEN
   &camera_rwebcam,
#endif
#ifdef ANDROID
   &camera_android,
#endif
#if defined(MAC_OS_X_VERSION_10_7) || defined(__IPHONE_4_0)
   &camera_apple,
#endif
   &camera_null,
   NULL,
};

/**
 * camera_driver_find_handle:
 * @index              : index of driver to get handle to.
 *
 * Returns: handle to camera driver at index. Can be NULL
 * if nothing found.
 **/
const void *camera_driver_find_handle(int index)
{
   const void *drv = camera_drivers[index];
   if (!drv)
      return NULL;
   return drv;
}

/**
 * camera_driver_find_ident:
 * @index              : index of driver to get handle to.
 *
 * Returns: Human-readable identifier of camera driver at index. Can be NULL
 * if nothing found.
 **/
const char *camera_driver_find_ident(int index)
{
   const camera_driver_t *drv = camera_drivers[index];
   if (!drv)
      return NULL;
   return drv->ident;
}

/**
 * config_get_camera_driver_options:
 *
 * Get an enumerated list of all camera driver names,
 * separated by '|'.
 *
 * Returns: string listing of all camera driver names,
 * separated by '|'.
 **/
const char* config_get_camera_driver_options(void)
{
   union string_list_elem_attr attr;
   unsigned i;
   char *options = NULL;
   int options_len = 0;
   struct string_list *options_l = string_list_new();

   attr.i = 0;

   for (i = 0; camera_driver_find_handle(i); i++)
   {
      const char *opt = camera_driver_find_ident(i);
      options_len += strlen(opt) + 1;
      string_list_append(options_l, opt, attr);
   }

   options = (char*)calloc(options_len, sizeof(char));

   string_list_join_concat(options, options_len, options_l, "|");

   string_list_free(options_l);
   options_l = NULL;

   return options;
}

void find_camera_driver(void)
{
   int i = find_driver_index("camera_driver", g_settings.camera.driver);
   if (i >= 0)
      driver.camera = (const camera_driver_t*)camera_driver_find_handle(i);
   else
   {
      unsigned d;
      RARCH_ERR("Couldn't find any camera driver named \"%s\"\n",
            g_settings.camera.driver);
      RARCH_LOG_OUTPUT("Available camera drivers are:\n");
      for (d = 0; camera_driver_find_handle(d); d++)
         RARCH_LOG_OUTPUT("\t%s\n", camera_driver_find_ident(d));
       
      RARCH_WARN("Going to default to first camera driver...\n");
       
      driver.camera = (const camera_driver_t*)camera_driver_find_handle(0);
       
      if (!driver.camera)
         rarch_fail(1, "find_camera_driver()");
   }
}

/**
 * driver_camera_start:
 *
 * Starts camera driver interface.
 * Used by RETRO_ENVIRONMENT_GET_CAMERA_INTERFACE.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool driver_camera_start(void)
{
   if (driver.camera && driver.camera_data && driver.camera->start)
   {
      if (g_settings.camera.allow)
         return driver.camera->start(driver.camera_data);

      msg_queue_push(g_extern.msg_queue,
            "Camera is explicitly disabled.\n", 1, 180);
   }
   return false;
}

/**
 * driver_camera_stop:
 *
 * Stops camera driver.
 * Used by RETRO_ENVIRONMENT_GET_CAMERA_INTERFACE.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
void driver_camera_stop(void)
{
   if (driver.camera && driver.camera->stop && driver.camera_data)
      driver.camera->stop(driver.camera_data);
}

/**
 * driver_camera_poll:
 *
 * Call camera driver's poll function.
 * Used by RETRO_ENVIRONMENT_GET_CAMERA_INTERFACE.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
void driver_camera_poll(void)
{
   if (driver.camera && driver.camera->poll && driver.camera_data)
      driver.camera->poll(driver.camera_data,
            g_extern.system.camera_callback.frame_raw_framebuffer,
            g_extern.system.camera_callback.frame_opengl_texture);
}

void init_camera(void)
{
   /* Resource leaks will follow if camera is initialized twice. */
   if (driver.camera_data)
      return;

   find_camera_driver();

   driver.camera_data = driver.camera->init(
         *g_settings.camera.device ? g_settings.camera.device : NULL,
         g_extern.system.camera_callback.caps,
         g_settings.camera.width ?
         g_settings.camera.width : g_extern.system.camera_callback.width,
         g_settings.camera.height ?
         g_settings.camera.height : g_extern.system.camera_callback.height);

   if (!driver.camera_data)
   {
      RARCH_ERR("Failed to initialize camera driver. Will continue without camera.\n");
      driver.camera_active = false;
   }

   if (g_extern.system.camera_callback.initialized)
      g_extern.system.camera_callback.initialized();
}

void uninit_camera(void)
{
   if (driver.camera_data && driver.camera)
   {
      if (g_extern.system.camera_callback.deinitialized)
         g_extern.system.camera_callback.deinitialized();

      if (driver.camera->free)
         driver.camera->free(driver.camera_data);
   }
   driver.camera_data = NULL;
}
