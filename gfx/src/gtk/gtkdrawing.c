/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Brian Ryner <bryner@netscape.com>  (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * This file contains painting functions for each of the gtk widgets.
 * Adapted from the gtk+ 1.2.10 source.
 */

#include <gtk/gtk.h>
#include <gdk/gdkprivate.h>
#include <string.h>
#include "gtkdrawing.h"

extern GtkWidget* gButtonWidget;
extern GtkWidget* gCheckboxWidget;
extern GtkWidget* gScrollbarWidget;
extern GtkWidget* gGripperWidget;
extern GtkWidget* gEntryWidget;
extern GtkWidget* gArrowWidget;
extern GtkWidget* gDropdownButtonWidget;
extern GtkWidget* gHandleBoxWidget;
extern GtkWidget* gFrameWidget;
extern GtkWidget* gProtoWindow;
extern GtkWidget* gProgressWidget;
extern GtkWidget* gTabWidget;
extern GtkTooltips* gTooltipWidget;

GtkStateType
ConvertGtkState(GtkWidgetState* state)
{
  if (state->disabled)
    return GTK_STATE_INSENSITIVE;
  else if (state->inHover)
    return (state->active ? GTK_STATE_ACTIVE : GTK_STATE_PRELIGHT);
  else
    return GTK_STATE_NORMAL;
}

void
moz_gtk_button_paint(GdkWindow* window, GtkStyle* style,
                     GdkRectangle* rect, GdkRectangle* cliprect,
                     GtkWidgetState* state, GtkReliefStyle relief)
{
  GtkShadowType shadow_type;
  gint default_spacing = 7; /* xxx fix me */
  GtkStateType button_state = ConvertGtkState(state);
  gint x = rect->x, y=rect->y, width=rect->width, height=rect->height;

  if (((GdkWindowPrivate*)window)->mapped) {
    gdk_window_set_back_pixmap(window, NULL, TRUE);
    gdk_window_clear_area(window, cliprect->x, cliprect->y, cliprect->width,
                          cliprect->height);
  }

  gtk_widget_set_state(gButtonWidget, button_state);
  if (state->isDefault)
    gtk_paint_box(style, window, GTK_STATE_NORMAL, GTK_SHADOW_IN, cliprect,
                  gButtonWidget, "buttondefault", x, y, width, height);

  if (state->canDefault) {
    x += style->klass->xthickness;
    y += style->klass->ythickness;
    width -= 2 * x + default_spacing;
    height -= 2 * y + default_spacing;
    x += (1 + default_spacing) / 2;
    y += (1 + default_spacing) / 2;
  }
       
  if (state->focused) {
    x += 1;
    y += 1;
    width -= 2;
    height -= 2;
  }
	
  shadow_type = (state->active && state->inHover) ? GTK_SHADOW_IN : GTK_SHADOW_OUT;
  
  if (relief != GTK_RELIEF_NONE || (button_state != GTK_STATE_NORMAL &&
                                    button_state != GTK_STATE_INSENSITIVE))
    gtk_paint_box(style, window, button_state, shadow_type, cliprect,
                  gButtonWidget, "button", x, y, width, height);
  
  if (state->focused) {
    x -= 1;
    y -= 1;
    width += 2;
    height += 2;
    
    gtk_paint_focus(style, window, cliprect, gButtonWidget, "button", x, y,
                    width - 1, height - 1);
  }
}

void
moz_gtk_checkbox_paint(GdkWindow* window, GtkStyle* style,
                       GdkRectangle* rect, GdkRectangle* cliprect,
                       GtkWidgetState* state, gboolean selected,
                       gboolean isradio)
{
  GtkStateType state_type;
  GtkShadowType shadow_type;
  gint indicator_size;
  gint x, y, width, height;

  _gtk_check_button_get_props(GTK_CHECK_BUTTON(gCheckboxWidget),
                              &indicator_size, NULL);

  /* left justified, vertically centered within the rect */
  x = rect->x;
  y = rect->y + (rect->height - indicator_size) / 2;
  width = indicator_size;
  height = indicator_size;
  
  if (selected) {
    state_type = GTK_STATE_ACTIVE;
    shadow_type = GTK_SHADOW_IN;
  }
  else {
    shadow_type = GTK_SHADOW_OUT;
    state_type = ConvertGtkState(state);
  }
  
  if (isradio)
    gtk_paint_option(style, window, state_type, shadow_type, cliprect,
                     gCheckboxWidget, "radiobutton", x, y, width, height);
  else
    gtk_paint_check(style, window, state_type, shadow_type, cliprect, 
                     gCheckboxWidget, "checkbutton", x, y, width, height);
}

void
calculate_arrow_dimensions(GdkRectangle* rect, GdkRectangle* arrow_rect)
{
  GtkMisc* misc = GTK_MISC(gArrowWidget);
  gint extent = MIN(rect->width - misc->xpad * 2, rect->height - misc->ypad * 2);
  arrow_rect->x = ((rect->x + misc->xpad) * (1.0 - misc->xalign) +
                   (rect->x + rect->width - extent - misc->xpad) * misc->xalign);
  arrow_rect->y = ((rect->y + misc->ypad) * (1.0 - misc->yalign) +
                   (rect->y + rect->height - extent - misc->ypad) * misc->yalign);

  arrow_rect->width = arrow_rect->height = extent;
}

void
moz_gtk_scrollbar_button_paint(GdkWindow* window, GtkStyle* style,
                               GdkRectangle* rect, GdkRectangle* cliprect,
                               GtkWidgetState* state, GtkArrowType type)
{
  GtkStateType state_type = ConvertGtkState(state);
  GtkShadowType shadow_type = (state->active) ? GTK_SHADOW_IN : GTK_SHADOW_OUT;
  GdkRectangle arrow_rect;
  calculate_arrow_dimensions(rect, &arrow_rect);
  gtk_paint_arrow(style, window, state_type, shadow_type, cliprect,
                  gScrollbarWidget, (type < 2) ? "vscrollbar" : "hscrollbar", 
                  type, TRUE, arrow_rect.x, arrow_rect.y, arrow_rect.width,
                  arrow_rect.height);
}

void
moz_gtk_scrollbar_trough_paint(GdkWindow* window, GtkStyle* style,
                               GdkRectangle* rect, GdkRectangle* cliprect,
                               GtkWidgetState* state)
{
  gtk_style_apply_default_background(style, window, TRUE, GTK_STATE_ACTIVE,
                                     cliprect, rect->x, rect->y,
                                     rect->width, rect->height);

  gtk_paint_box(style, window, GTK_STATE_ACTIVE, GTK_SHADOW_IN, cliprect,
                gScrollbarWidget, "trough", rect->x, rect->y,  rect->width,
                rect->height);

  if (state->focused)
    gtk_paint_focus(style, window, cliprect, gScrollbarWidget, "trough",
                    rect->x, rect->y, rect->width, rect->height);
}

void
moz_gtk_scrollbar_thumb_paint(GdkWindow* window, GtkStyle* style,
                              GdkRectangle* rect, GdkRectangle* cliprect,
                              GtkWidgetState* state)
{
  GtkStateType state_type = (state->inHover || state->active) ? GTK_STATE_PRELIGHT : GTK_STATE_NORMAL;
  gtk_paint_box(style, window, state_type, GTK_SHADOW_OUT, cliprect,
                gScrollbarWidget, "slider", rect->x, rect->y, rect->width, 
                rect->height);
}

#define RANGE_CLASS(w) GTK_RANGE_CLASS(GTK_OBJECT(w)->klass)

void
moz_gtk_get_scrollbar_metrics(gint* slider_width, gint* trough_border,
                              gint* stepper_size, gint* stepper_spacing,
                              gint* min_slider_size)
{
#if ((GTK_MINOR_VERSION == 2) && (GTK_MICRO_VERSION > 8)) || (GTK_MINOR_VERSION > 2)
  /*
   * This API is only supported in GTK+ >= 1.2.9, and gives per-theme values.
   */

  if (slider_width)
    *slider_width = gtk_style_get_prop_experimental(gScrollbarWidget->style,
                                                    "GtkRange::slider_width",
                                                    RANGE_CLASS(gScrollbarWidget)->slider_width);

  if (trough_border)
    *trough_border = gtk_style_get_prop_experimental(gScrollbarWidget->style,
                                                     "GtkRange::trough_border",
                                                     gScrollbarWidget->style->klass->xthickness);

  if (stepper_size)
    *stepper_size = gtk_style_get_prop_experimental(gScrollbarWidget->style,
                                                    "GtkRange::stepper_size",
                                                    RANGE_CLASS(gScrollbarWidget)->stepper_size);

  if (stepper_spacing)
    *stepper_spacing = gtk_style_get_prop_experimental(gScrollbarWidget->style,
                                                       "GtkRange::stepper_spacing",
                                                       RANGE_CLASS(gScrollbarWidget)->stepper_slider_spacing);

  if (min_slider_size)
    *min_slider_size = RANGE_CLASS(gScrollbarWidget)->min_slider_size;
#else
  /*
   * This is the older method, which gives per-engine values.
   */

  if (slider_width)
    *slider_width = RANGE_CLASS(gScrollbarWidget)->slider_width;

  if (trough_border)
    *trough_border = gScrollbarWidget->style->klass->xthickness;

  if (stepper_size)
    *stepper_size = RANGE_CLASS(gScrollbarWidget)->stepper_size;

  if (stepper_spacing)
    *stepper_spacing = RANGE_CLASS(gScrollbarWidget)->stepper_slider_spacing;

  if (min_slider_size)
    *min_slider_size = RANGE_CLASS(gScrollbarWidget)->min_slider_size;
#endif
}

void
moz_gtk_gripper_paint(GdkWindow* window, GtkStyle* style, GdkRectangle* rect,
                      GdkRectangle* cliprect, GtkWidgetState* state)
{
  GtkStateType state_type = ConvertGtkState(state);
  GtkShadowType shadow_type = GTK_HANDLE_BOX(gGripperWidget)->shadow_type;

  gtk_paint_box(style, window, state_type, shadow_type, cliprect,
                gGripperWidget, "handlebox_bin", rect->x, rect->y,
                rect->width, rect->height);
}

void
moz_gtk_entry_paint(GdkWindow* window, GtkStyle* style, GdkRectangle* rect,
                    GdkRectangle* cliprect, GtkWidgetState* state)
{
  gint x = rect->x, y = rect->y, width = rect->width, height = rect->height;
  
  if (state->focused) {
      x += 1;
      y += 1;
      width -= 2;
      height -= 2;
  }
  
  gtk_paint_shadow (style, window, GTK_STATE_NORMAL, GTK_SHADOW_IN, cliprect,
                    gEntryWidget, "entry", x, y, width, height);
  
  if (state->focused)
      gtk_paint_focus (style, window,  cliprect, gEntryWidget, "entry",
                       rect->x, rect->y, rect->width - 1, rect->height - 1);

  x = style->klass->xthickness;
  y = style->klass->ythickness;

  gtk_paint_flat_box (style, window, GTK_STATE_NORMAL, GTK_SHADOW_NONE,
                      cliprect, gEntryWidget, "entry_bg",  rect->x + x,
                      rect->y + y, rect->width - 2*x, rect->height - 2*y);
}

void
moz_gtk_dropdown_arrow_paint(GdkWindow* window, GtkStyle* style,
                             GdkRectangle* rect, GdkRectangle* cliprect, 
                             GtkWidgetState* state)
{
  GdkRectangle arrow_rect, real_arrow_rect;
  GtkStateType state_type = ConvertGtkState(state);
  GtkShadowType shadow_type = state->active ? GTK_SHADOW_IN : GTK_SHADOW_OUT;

  moz_gtk_button_paint(window, gDropdownButtonWidget->style, rect, cliprect,
                       state, GTK_RELIEF_NORMAL);

  /* This mirrors gtkbutton's child positioning */
  arrow_rect.x = rect->x + 1 + gDropdownButtonWidget->style->klass->xthickness;
  arrow_rect.y = rect->y + 1 + gDropdownButtonWidget->style->klass->ythickness;
  arrow_rect.width = MAX(1, rect->width - (arrow_rect.x - rect->x) * 2);
  arrow_rect.height = MAX(1, rect->height - (arrow_rect.y - rect->y) * 2);

  calculate_arrow_dimensions(&arrow_rect, &real_arrow_rect);
  gtk_paint_arrow(style, window, state_type, shadow_type, cliprect,
                  gScrollbarWidget, "arrow",  GTK_ARROW_DOWN, TRUE,
                  real_arrow_rect.x, real_arrow_rect.y,
                  real_arrow_rect.width, real_arrow_rect.height);
}

void
moz_gtk_container_paint(GdkWindow* window, GtkStyle* style, GdkRectangle* rect,
                        GdkRectangle* cliprect, GtkWidgetState* state, 
                        gboolean isradio)
{
  GtkStateType state_type = ConvertGtkState(state);

  if (state_type != GTK_STATE_NORMAL && state_type != GTK_STATE_PRELIGHT)
    state_type = GTK_STATE_NORMAL;
  
  if (state_type != GTK_STATE_NORMAL) /* this is for drawing a prelight box */
    gtk_paint_flat_box(style, window, state_type, GTK_SHADOW_ETCHED_OUT,
                       cliprect, gCheckboxWidget,
                       isradio ? "radiobutton" : "checkbutton",
                       rect->x, rect->y, rect->width, rect->height);

  if (state->focused)
    gtk_paint_focus(style, window, cliprect, gCheckboxWidget, "checkbutton",
                    rect->x, rect->y, rect->width - 1, rect->height - 1);
}

void
moz_gtk_toolbar_paint(GdkWindow* window, GtkStyle* style, GdkRectangle* rect,
                      GdkRectangle* cliprect)
{
  gtk_paint_box(style, window, GTK_STATE_NORMAL, GTK_SHADOW_OUT,
                cliprect, gHandleBoxWidget, "dockitem_bin",
                rect->x, rect->y, rect->width, rect->height);
}

void
moz_gtk_tooltip_paint(GdkWindow* window, GtkStyle* style, GdkRectangle* rect,
                      GdkRectangle* cliprect)
{
  gtk_paint_flat_box(style, window, GTK_STATE_NORMAL, GTK_SHADOW_OUT,
                     cliprect, gTooltipWidget->tip_window, "tooltip", rect->x,
                     rect->y, rect->width, rect->height);
}

void
moz_gtk_frame_paint(GdkWindow* window, GtkStyle* style, GdkRectangle* rect,
                    GdkRectangle* cliprect)
{
  gtk_paint_flat_box(gProtoWindow->style, window, GTK_STATE_NORMAL, GTK_SHADOW_NONE,
                     NULL, gProtoWindow, "base", rect->x, rect->y,
                     rect->width, rect->height);

  gtk_paint_shadow(style, window, GTK_STATE_NORMAL, GTK_SHADOW_IN, cliprect,
                   gFrameWidget, "frame", rect->x, rect->y, rect->width,
                   rect->height);
}

void
moz_gtk_progressbar_paint(GdkWindow* window, GtkStyle* style,
                          GdkRectangle* rect, GdkRectangle* cliprect)
{
  gtk_paint_box(style, window, GTK_STATE_NORMAL, GTK_SHADOW_IN,
                cliprect, gProgressWidget, "trough", rect->x, rect->y,
                rect->width, rect->height);
}

void
moz_gtk_progress_chunk_paint(GdkWindow* window, GtkStyle* style,
                             GdkRectangle* rect, GdkRectangle* cliprect)
{
  gtk_paint_box(style, window, GTK_STATE_PRELIGHT, GTK_SHADOW_OUT,
                cliprect, gProgressWidget, "bar", rect->x, rect->y,
                rect->width, rect->height);
}

void
moz_gtk_tab_paint(GdkWindow* window, GtkStyle* style, GdkRectangle* rect,
                  GdkRectangle* cliprect, GtkTabType type)
{
  gtk_paint_extension(style, window,
                      (type == kTabSelected ? GTK_STATE_NORMAL : GTK_STATE_ACTIVE),
                      GTK_SHADOW_OUT, cliprect, gTabWidget, "tab", rect->x,
                      rect->y, rect->width, rect->height, GTK_POS_BOTTOM);

  /* Here is the sleazy hack for before/after selected tab */
  if (type == kTabBeforeSelected)
    gtk_paint_extension(style, window, GTK_STATE_NORMAL, GTK_SHADOW_OUT,
                        cliprect, gTabWidget, "tab", rect->x + rect->width,
                        rect->y, rect->width, rect->height, GTK_POS_BOTTOM);
  else if (type == kTabAfterSelected)
    gtk_paint_extension(style, window, GTK_STATE_NORMAL, GTK_SHADOW_OUT,
                        cliprect, gTabWidget, "tab", rect->x - rect->width,
                        rect->y, rect->width, rect->height, GTK_POS_BOTTOM);
}

void
moz_gtk_tabpanels_paint(GdkWindow* window, GtkStyle* style,
                        GdkRectangle* rect, GdkRectangle* cliprect)
{
  gtk_paint_box(style, window, GTK_STATE_NORMAL, GTK_SHADOW_OUT,
                cliprect, gTabWidget, "notebook", rect->x, rect->y,
                rect->width, rect->height);
}

