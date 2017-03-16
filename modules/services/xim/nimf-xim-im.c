/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 2; tab-width: 2 -*- */
/*
 * nimf-xim-im.c
 * This file is part of Nimf.
 *
 * Copyright (C) 2015-2017 Hodong Kim <cogniti@gmail.com>
 *
 * Nimf is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Nimf is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program;  If not, see <http://www.gnu.org/licenses/>.
 */

#include "nimf-xim-im.h"
#include <X11/Xutil.h>
#include "IMdkit/Xi18n.h"

G_DEFINE_TYPE (NimfXimIM, nimf_xim_im, NIMF_TYPE_SERVICE_IM);

void
nimf_xim_im_emit_commit (NimfServiceIM *im,
                         const gchar   *text)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  XIMS xims = im->cb_user_data;
  XTextProperty property;
  Xutf8TextListToTextProperty (xims->core.display,
                               (char **)&text, 1, XCompoundTextStyle,
                               &property);

  IMCommitStruct commit_data = {0};
  commit_data.major_code = XIM_COMMIT;
  commit_data.connect_id = im->xim_connect_id;
  commit_data.icid       = im->icid;
  commit_data.flag       = XimLookupChars;
  commit_data.commit_string = (gchar *) property.value;
  IMCommitString (xims, (XPointer) &commit_data);

  XFree (property.value);
}

void nimf_xim_im_emit_preedit_start (NimfServiceIM *im)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  XIMS xims = im->cb_user_data;
  IMPreeditStateStruct preedit_state_data = {0};
  preedit_state_data.connect_id = im->xim_connect_id;
  preedit_state_data.icid       = im->icid;
  IMPreeditStart (xims, (XPointer) &preedit_state_data);
  IMPreeditCBStruct preedit_cb_data = {0};
  preedit_cb_data.major_code = XIM_PREEDIT_START;
  preedit_cb_data.connect_id = im->xim_connect_id;
  preedit_cb_data.icid       = im->icid;
  IMCallCallback (xims, (XPointer) &preedit_cb_data);
}

void
nimf_xim_im_emit_preedit_changed (NimfServiceIM    *im,
                                  const gchar      *preedit_string,
                                  NimfPreeditAttr **attrs,
                                  gint              cursor_pos)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  XIMS              xims = im->cb_user_data;
  IMPreeditCBStruct preedit_cb_data = {0};
  XIMText           text;
  XTextProperty     text_property;

  static XIMFeedback *feedback;
  gint i, j, len;

  len = g_utf8_strlen (preedit_string, -1);
  feedback = g_malloc0 (sizeof (XIMFeedback) * (len + 1));

  for (i = 0; attrs[i]; i++)
  {
    switch (attrs[i]->type)
    {
      case NIMF_PREEDIT_ATTR_HIGHLIGHT:
        for (j = attrs[i]->start_index; j < attrs[i]->end_index; j++)
          feedback[j] |= XIMHighlight;
        break;
      case NIMF_PREEDIT_ATTR_UNDERLINE:
        for (j = attrs[i]->start_index; j < attrs[i]->end_index; j++)
          feedback[j] |= XIMUnderline;
        break;
      default:
        break;
    }
  }

  feedback[len] = 0;

  preedit_cb_data.major_code = XIM_PREEDIT_DRAW;
  preedit_cb_data.connect_id = im->xim_connect_id;
  preedit_cb_data.icid = im->icid;
  preedit_cb_data.todo.draw.caret = len;
  preedit_cb_data.todo.draw.chg_first = 0;
  preedit_cb_data.todo.draw.chg_length = im->xim_preedit_length;
  preedit_cb_data.todo.draw.text = &text;

  text.feedback = feedback;

  if (len > 0)
  {
    Xutf8TextListToTextProperty (xims->core.display,
                                 (char **) &preedit_string, 1,
                                 XCompoundTextStyle, &text_property);
    text.encoding_is_wchar = 0;
    text.length = strlen ((char *) text_property.value);
    text.string.multi_byte = (char *) text_property.value;
    IMCallCallback (xims, (XPointer) &preedit_cb_data);
    XFree (text_property.value);
  }
  else
  {
    text.encoding_is_wchar = 0;
    text.length = 0;
    text.string.multi_byte = "";
    IMCallCallback (xims, (XPointer) &preedit_cb_data);
    len = 0;
  }

  im->xim_preedit_length = len;

  g_free (feedback);
}

void nimf_xim_im_emit_preedit_end (NimfServiceIM *im)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  XIMS xims = im->cb_user_data;
  IMPreeditStateStruct preedit_state_data = {0};
  preedit_state_data.connect_id = im->xim_connect_id;
  preedit_state_data.icid       = im->icid;
  IMPreeditEnd (xims, (XPointer) &preedit_state_data);

  IMPreeditCBStruct preedit_cb_data = {0};
  preedit_cb_data.major_code = XIM_PREEDIT_DONE;
  preedit_cb_data.connect_id = im->xim_connect_id;
  preedit_cb_data.icid       = im->icid;
  IMCallCallback (xims, (XPointer) &preedit_cb_data);
}

NimfXimIM *nimf_xim_im_new (NimfConnection    *connection,
                            NimfServer        *server,
                            gpointer           cb_user_data)
{
  return g_object_new (NIMF_TYPE_XIM_IM,
                       "connection",      connection,
                       "server",          server,
                       "cb-user-data",    cb_user_data,
                       NULL);
}

static void
nimf_xim_im_init (NimfXimIM *nimf_xim_im)
{
}

static void
nimf_xim_im_finalize (GObject *object)
{

  G_OBJECT_CLASS (nimf_xim_im_parent_class)->finalize (object);
}

static void
nimf_xim_im_class_init (NimfXimIMClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  NimfServiceIMClass *service_im_class = NIMF_SERVICE_IM_CLASS (class);

  object_class->finalize = nimf_xim_im_finalize;

  service_im_class->emit_commit          = nimf_xim_im_emit_commit;
  service_im_class->emit_preedit_start   = nimf_xim_im_emit_preedit_start;
  service_im_class->emit_preedit_changed = nimf_xim_im_emit_preedit_changed;
  service_im_class->emit_preedit_end     = nimf_xim_im_emit_preedit_end;
}
