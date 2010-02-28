/*
    DeaDBeeF - ultimate music player for GNU/Linux systems with X11
    Copyright (C) 2009-2010 Alexey Yakovenko <waker@users.sourceforge.net>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifdef HAVE_CONFIG_H
#  include "../../config.h"
#endif
#if HAVE_NOTIFY
#include <libnotify/notify.h>
#endif

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <ctype.h>
#include <assert.h>
#include <sys/time.h>
#include "ddblistview.h"
#include "drawing.h"
//#include "callbacks.h"
//#include "interface.h"
//#include "support.h"
//#include "search.h"
//#include "progress.h"
//#include "../../session.h"
//#include "parser.h"
//#include "gtkui.h"

#define min(x,y) ((x)<(y)?(x):(y))
#define max(x,y) ((x)>(y)?(x):(y))

#define DEFAULT_GROUP_TITLE_HEIGHT 30
int GROUP_TITLE_HEIGHT = 0;
#define SCROLL_STEP 20
#define AUTOSCROLL_UPDATE_FREQ 0.01f

#define trace(...) { fprintf(stderr, __VA_ARGS__); }
//#define trace(fmt,...)

#define PL_NEXT(it) (ps->binding->next (it))
#define PL_PREV(it) (ps->binding->prev (it))
#define REF(it) {if (it) ps->binding->ref (it);}
#define UNREF(it) {if (it) ps->binding->unref(it);}

// HACK!!
extern GtkWidget *theme_treeview;

G_DEFINE_TYPE (DdbListview, ddb_listview, GTK_TYPE_TABLE);

struct _DdbListviewColumn {
    char *title;
    int width;
    struct _DdbListviewColumn *next;
    void *user_data;
    unsigned align_right : 1;
    unsigned sort_order : 2; // 0=none, 1=asc, 2=desc
};
typedef struct _DdbListviewColumn DdbListviewColumn;

struct _DdbListviewGroup {
    DdbListviewIter head;
    uint16_t num_items;
    uint8_t _padding;
    struct _DdbListviewGroup *next;
};
typedef struct _DdbListviewGroup DdbListviewGroup;

static void ddb_listview_class_init(DdbListviewClass *klass);
static void ddb_listview_init(DdbListview *listview);
static void ddb_listview_size_request(GtkWidget *widget,
        GtkRequisition *requisition);
static void ddb_listview_size_allocate(GtkWidget *widget,
        GtkAllocation *allocation);
static void ddb_listview_realize(GtkWidget *widget);
static void ddb_listview_paint(GtkWidget *widget);
static void ddb_listview_destroy(GtkObject *object);

// fwd decls
static inline void
draw_drawable (GdkDrawable *window, GdkGC *gc, GdkDrawable *drawable, int x1, int y1, int x2, int y2, int w, int h);

////// list functions ////
void
ddb_listview_list_render (DdbListview *ps, int x, int y, int w, int h);
void
ddb_listview_list_expose (DdbListview *ps, int x, int y, int w, int h);
void
ddb_listview_list_render_row_background (DdbListview *ps, DdbListviewIter it, int even, int cursor, int x, int y, int w, int h);
void
ddb_listview_list_render_row_foreground (DdbListview *ps, DdbListviewIter it, int even, int cursor, int x, int y, int w, int h);
void
ddb_listview_list_render_row (DdbListview *ps, int row, DdbListviewIter it, int expose);
void
ddb_listview_list_track_dragdrop (DdbListview *ps, int y);
void
ddb_listview_list_mousemove (DdbListview *ps, GdkEventMotion *event);
void
ddb_listview_list_setup_vscroll (DdbListview *ps);
void
ddb_listview_list_setup_hscroll (DdbListview *ps);
void
ddb_listview_list_set_hscroll (DdbListview *ps, int newscroll);
void
ddb_listview_set_cursor (DdbListview *pl, int cursor);
int
ddb_listview_get_row_pos (DdbListview *listview, int pos);

////// header functions ////
void
ddb_listview_header_render (DdbListview *ps);
void
ddb_listview_header_expose (DdbListview *ps, int x, int y, int w, int h);

////// column management functions ////
void
ddb_listview_column_move (DdbListview *listview, DdbListviewColumn *which, int inspos);
void
ddb_listview_column_free (DdbListview *listview, DdbListviewColumn *c);


// signal handlers
void
ddb_listview_vscroll_value_changed            (GtkRange        *widget,
                                        gpointer         user_data);
void
ddb_listview_hscroll_value_changed           (GtkRange        *widget,
                                        gpointer         user_data);

void
ddb_listview_list_drag_data_received         (GtkWidget       *widget,
                                        GdkDragContext  *drag_context,
                                        gint             x,
                                        gint             y,
                                        GtkSelectionData *data,
                                        guint            info,
                                        guint            time,
                                        gpointer         user_data);

gboolean
ddb_listview_header_expose_event                 (GtkWidget       *widget,
                                        GdkEventExpose  *event,
                                        gpointer         user_data);

gboolean
ddb_listview_header_configure_event              (GtkWidget       *widget,
                                        GdkEventConfigure *event,
                                        gpointer         user_data);

void
ddb_listview_header_realize                      (GtkWidget       *widget,
                                        gpointer         user_data);

gboolean
ddb_listview_header_motion_notify_event          (GtkWidget       *widget,
                                        GdkEventMotion  *event,
                                        gpointer         user_data);

gboolean
ddb_listview_header_button_press_event           (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

gboolean
ddb_listview_header_button_release_event         (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

gboolean
ddb_listview_list_configure_event            (GtkWidget       *widget,
                                        GdkEventConfigure *event,
                                        gpointer         user_data);

gboolean
ddb_listview_list_expose_event               (GtkWidget       *widget,
                                        GdkEventExpose  *event,
                                        gpointer         user_data);

void
ddb_listview_list_realize                    (GtkWidget       *widget,
                                        gpointer         user_data);

gboolean
ddb_listview_list_button_press_event         (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

gboolean
ddb_listview_list_drag_motion                (GtkWidget       *widget,
                                        GdkDragContext  *drag_context,
                                        gint             x,
                                        gint             y,
                                        guint            time,
                                        gpointer         user_data);

gboolean
ddb_listview_list_drag_drop                  (GtkWidget       *widget,
                                        GdkDragContext  *drag_context,
                                        gint             x,
                                        gint             y,
                                        guint            time,
                                        gpointer         user_data);

void
ddb_listview_list_drag_data_get              (GtkWidget       *widget,
                                        GdkDragContext  *drag_context,
                                        GtkSelectionData *data,
                                        guint            info,
                                        guint            time,
                                        gpointer         user_data);

void
ddb_listview_list_drag_end                   (GtkWidget       *widget,
                                        GdkDragContext  *drag_context,
                                        gpointer         user_data);

gboolean
ddb_listview_list_drag_failed                (GtkWidget       *widget,
                                        GdkDragContext  *arg1,
                                        GtkDragResult    arg2,
                                        gpointer         user_data);

void
ddb_listview_list_drag_leave                 (GtkWidget       *widget,
                                        GdkDragContext  *drag_context,
                                        guint            time,
                                        gpointer         user_data);

gboolean
ddb_listview_list_button_release_event       (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

gboolean
ddb_listview_motion_notify_event        (GtkWidget       *widget,
                                        GdkEventMotion  *event,
                                        gpointer         user_data);
gboolean
ddb_listview_list_button_press_event         (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

gboolean
ddb_listview_vscroll_event               (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data);

gboolean
ddb_listview_list_button_release_event       (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

gboolean
ddb_listview_motion_notify_event        (GtkWidget       *widget,
                                        GdkEventMotion  *event,
                                        gpointer         user_data);


static void
ddb_listview_class_init(DdbListviewClass *class)
{
  GtkTableClass *widget_class;
  widget_class = (GtkTableClass *) class;
}

static void
ddb_listview_init(DdbListview *listview)
{
    // init instance - create all subwidgets, and insert into table

    listview->rowheight = draw_get_font_size () + 12;

    listview->col_movepos = -1;
    listview->drag_motion_y = -1;

    listview->scroll_mode = 0;
    listview->scroll_pointer_y = -1;
    listview->scroll_direction = 0;
    listview->scroll_active = 0;
    memset (&listview->tm_prevscroll, 0, sizeof (listview->tm_prevscroll));
    listview->scroll_sleep_time = 0;

    listview->areaselect = 0;
    listview->areaselect_x = -1;
    listview->areaselect_y = -1;
    listview->areaselect_dx = -1;
    listview->areaselect_dy = -1;
    listview->dragwait = 0;
    listview->shift_sel_anchor = -1;

    listview->header_dragging = -1;
    listview->header_sizing = -1;
    listview->header_dragpt[0] = 0;
    listview->header_dragpt[1] = 0;
    listview->last_header_motion_ev = -1; //is it subject to remove?
    listview->prev_header_x = -1;
    listview->header_prepare = 0;

    listview->columns = NULL;
    listview->groups = NULL;

    GtkWidget *hbox;
    GtkWidget *vbox;

    gtk_table_resize (GTK_TABLE (listview), 2, 2);
    listview->scrollbar = gtk_vscrollbar_new (GTK_ADJUSTMENT (gtk_adjustment_new (0, 0, 1, 1, 0, 0)));
    gtk_widget_show (listview->scrollbar);
    gtk_table_attach (GTK_TABLE (listview), listview->scrollbar, 1, 2, 0, 1,
            (GtkAttachOptions) (GTK_FILL),
            (GtkAttachOptions) (GTK_FILL), 0, 0);

    hbox = gtk_hbox_new (FALSE, 0);
    gtk_widget_show (hbox);
    gtk_table_attach (GTK_TABLE (listview), hbox, 0, 1, 0, 1,
            (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
            (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);

    vbox = gtk_vbox_new (FALSE, 0);
    gtk_widget_show (vbox);
    gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);

    listview->header = gtk_drawing_area_new ();
    gtk_widget_show (listview->header);
    gtk_box_pack_start (GTK_BOX (vbox), listview->header, FALSE, TRUE, 0);
    gtk_widget_set_size_request (listview->header, -1, 24);
    gtk_widget_set_events (listview->header, GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK | GDK_BUTTON_MOTION_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);

    listview->list = gtk_drawing_area_new ();
    gtk_widget_show (listview->list);
    gtk_box_pack_start (GTK_BOX (vbox), listview->list, TRUE, TRUE, 0);
    gtk_widget_set_events (listview->list, GDK_EXPOSURE_MASK | GDK_POINTER_MOTION_MASK | GDK_BUTTON_MOTION_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK);

    listview->hscrollbar = gtk_hscrollbar_new (GTK_ADJUSTMENT (gtk_adjustment_new (0, 0, 0, 0, 0, 0)));
    gtk_widget_show (listview->hscrollbar);
    gtk_table_attach (GTK_TABLE (listview), listview->hscrollbar, 0, 1, 1, 2,
            (GtkAttachOptions) (GTK_FILL),
            (GtkAttachOptions) (GTK_FILL), 0, 0);


    gtk_object_set_data (GTK_OBJECT (listview->list), "owner", listview);
    gtk_object_set_data (GTK_OBJECT (listview->header), "owner", listview);
    gtk_object_set_data (GTK_OBJECT (listview->scrollbar), "owner", listview);
    gtk_object_set_data (GTK_OBJECT (listview->hscrollbar), "owner", listview);

    g_signal_connect ((gpointer) listview->list, "configure_event",
            G_CALLBACK (ddb_listview_list_configure_event),
            NULL);

    g_signal_connect ((gpointer) listview->scrollbar, "value_changed",
            G_CALLBACK (ddb_listview_vscroll_value_changed),
            NULL);
    g_signal_connect ((gpointer) listview->header, "expose_event",
            G_CALLBACK (ddb_listview_header_expose_event),
            NULL);
    g_signal_connect ((gpointer) listview->header, "configure_event",
            G_CALLBACK (ddb_listview_header_configure_event),
            NULL);
    g_signal_connect ((gpointer) listview->header, "realize",
            G_CALLBACK (ddb_listview_header_realize),
            NULL);
    g_signal_connect ((gpointer) listview->header, "motion_notify_event",
            G_CALLBACK (ddb_listview_header_motion_notify_event),
            NULL);
    g_signal_connect ((gpointer) listview->header, "button_press_event",
            G_CALLBACK (ddb_listview_header_button_press_event),
            NULL);
    g_signal_connect ((gpointer) listview->header, "button_release_event",
            G_CALLBACK (ddb_listview_header_button_release_event),
            NULL);
    g_signal_connect ((gpointer) listview->list, "expose_event",
            G_CALLBACK (ddb_listview_list_expose_event),
            NULL);
    g_signal_connect ((gpointer) listview->list, "realize",
            G_CALLBACK (ddb_listview_list_realize),
            NULL);
    g_signal_connect ((gpointer) listview->list, "button_press_event",
            G_CALLBACK (ddb_listview_list_button_press_event),
            NULL);
    g_signal_connect ((gpointer) listview->list, "scroll_event",
            G_CALLBACK (ddb_listview_vscroll_event),
            NULL);
//    g_signal_connect ((gpointer) listview->list, "drag_begin",
//            G_CALLBACK (on_list_drag_begin),
//            NULL);
    g_signal_connect ((gpointer) listview->list, "drag_motion",
            G_CALLBACK (ddb_listview_list_drag_motion),
            NULL);
    g_signal_connect ((gpointer) listview->list, "drag_drop",
            G_CALLBACK (ddb_listview_list_drag_drop),
            NULL);
    g_signal_connect ((gpointer) listview->list, "drag_data_get",
            G_CALLBACK (ddb_listview_list_drag_data_get),
            NULL);
    g_signal_connect ((gpointer) listview->list, "drag_end",
            G_CALLBACK (ddb_listview_list_drag_end),
            NULL);
    g_signal_connect ((gpointer) listview->list, "drag_failed",
            G_CALLBACK (ddb_listview_list_drag_failed),
            NULL);
    g_signal_connect ((gpointer) listview->list, "drag_leave",
            G_CALLBACK (ddb_listview_list_drag_leave),
            NULL);
    g_signal_connect ((gpointer) listview->list, "button_release_event",
            G_CALLBACK (ddb_listview_list_button_release_event),
            NULL);
    g_signal_connect ((gpointer) listview->list, "motion_notify_event",
            G_CALLBACK (ddb_listview_motion_notify_event),
            NULL);
    g_signal_connect ((gpointer) listview->list, "drag_data_received",
            G_CALLBACK (ddb_listview_list_drag_data_received),
            NULL);
    g_signal_connect ((gpointer) listview->hscrollbar, "value_changed",
            G_CALLBACK (ddb_listview_hscroll_value_changed),
            NULL);
}

GtkWidget * ddb_listview_new()
{
   return GTK_WIDGET(gtk_type_new(ddb_listview_get_type()));
}

static void
ddb_listview_destroy(GtkObject *object)
{
  DdbListview *listview;
  DdbListviewClass *class;

  g_return_if_fail(object != NULL);
  g_return_if_fail(DDB_IS_LISTVIEW(object));

  listview = DDB_LISTVIEW(object);
  while (listview->columns) {
      DdbListviewColumn *next = listview->columns->next;
      ddb_listview_column_free (listview, listview->columns);
      listview->columns = next;
  }

  class = gtk_type_class(gtk_widget_get_type());

  if (GTK_OBJECT_CLASS(class)->destroy) {
     (* GTK_OBJECT_CLASS(class)->destroy) (object);
  }
}

void
ddb_listview_refresh (DdbListview *listview, uint32_t flags) {
    if (flags & DDB_REFRESH_LIST) {
        int height = listview->fullheight;
        ddb_listview_build_groups (listview);
        if (height != listview->fullheight) {
            flags |= DDB_REFRESH_VSCROLL;
        }
        ddb_listview_list_render (listview, 0, 0, listview->list->allocation.width, listview->list->allocation.height);
    }
    if (flags & DDB_REFRESH_VSCROLL) {
        ddb_listview_list_setup_vscroll (listview);
    }
    if (flags & DDB_REFRESH_HSCROLL) {
        ddb_listview_list_setup_hscroll (listview);
    }
    if (flags & DDB_REFRESH_COLUMNS) {
        ddb_listview_header_render (listview);
    }
    if (flags & DDB_EXPOSE_COLUMNS) {
        ddb_listview_header_expose (listview, 0, 0, listview->header->allocation.width, listview->header->allocation.height);
    }
    if (flags & DDB_EXPOSE_LIST) {
        ddb_listview_list_expose (listview, 0, 0, listview->list->allocation.width, listview->list->allocation.height);
    }
}

void
ddb_listview_list_realize                    (GtkWidget       *widget,
        gpointer         user_data)
{
    GtkTargetEntry entry = {
        .target = "STRING",
        .flags = GTK_TARGET_SAME_WIDGET/* | GTK_TARGET_OTHER_APP*/,
        TARGET_SAMEWIDGET
    };
    // setup drag-drop source
//    gtk_drag_source_set (widget, GDK_BUTTON1_MASK, &entry, 1, GDK_ACTION_MOVE);
    // setup drag-drop target
    gtk_drag_dest_set (widget, GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_DROP, &entry, 1, GDK_ACTION_COPY | GDK_ACTION_MOVE);
    gtk_drag_dest_add_uri_targets (widget);
//    gtk_drag_dest_set_track_motion (widget, TRUE);
}

gboolean
ddb_listview_list_configure_event            (GtkWidget       *widget,
        GdkEventConfigure *event,
        gpointer         user_data)
{
    DdbListview *ps = DDB_LISTVIEW (gtk_object_get_data (GTK_OBJECT (widget), "owner"));
    ddb_listview_list_setup_vscroll (ps);
    ddb_listview_list_setup_hscroll (ps);
    widget = ps->list;
    if (ps->backbuf) {
        g_object_unref (ps->backbuf);
        ps->backbuf = NULL;
    }
    ps->backbuf = gdk_pixmap_new (widget->window, widget->allocation.width, widget->allocation.height, -1);

    ddb_listview_list_render (ps, 0, 0, widget->allocation.width, widget->allocation.height);
    return FALSE;
}

// returns Y coordinate of an item by its index
int
ddb_listview_get_row_pos (DdbListview *listview, int row_idx) {
    int y = 0;
    int idx = 0;
    DdbListviewGroup *grp = listview->groups;
    while (grp) {
        if (idx + grp->num_items > row_idx) {
            return y + GROUP_TITLE_HEIGHT + (row_idx - idx) * listview->rowheight;
        }
        y += GROUP_TITLE_HEIGHT + grp->num_items * listview->rowheight;
        idx += grp->num_items;
        grp = grp->next;
    }
}

// input: absolute y coord in list (not in window)
// returns -1 if nothing was hit, otherwise returns pointer to a group, and item idx
// item idx may be set to -1 if group title was hit
static int
ddb_listview_list_pickpoint_y (DdbListview *listview, int y, DdbListviewGroup **group, int *group_idx, int *global_idx) {
    int idx = 0;
    int grp_y = 0;
    int gidx = 0;
    DdbListviewGroup *grp = listview->groups;
    while (grp) {
        int h = GROUP_TITLE_HEIGHT + grp->num_items * listview->rowheight;
        if (y >= grp_y && y < grp_y + h) {
            *group = grp;
            y -= grp_y;
            if (y < GROUP_TITLE_HEIGHT) {
                *group_idx = -1;
                *global_idx = -1;
            }
            else {
                *group_idx = (y - GROUP_TITLE_HEIGHT) / listview->rowheight;
                *global_idx = idx + *group_idx;
            }
            return 0;
        }
        grp_y += GROUP_TITLE_HEIGHT + grp->num_items * listview->rowheight;
        idx += grp->num_items;
        grp = grp->next;
        gidx++;
    }
    return -1;
}

void
ddb_listview_list_render (DdbListview *listview, int x, int y, int w, int h) {
    if (!listview->backbuf) {
        return;
    }
    int idx = 0;
    // find 1st group
    DdbListviewGroup *grp = listview->groups;
    int grp_y = 0;
    while (grp && grp_y + GROUP_TITLE_HEIGHT + grp->num_items * listview->rowheight < y + listview->scrollpos) {
        grp_y += GROUP_TITLE_HEIGHT + grp->num_items * listview->rowheight;
        idx += grp->num_items + 1;
        grp = grp->next;
    }
    draw_begin ((uintptr_t)listview->backbuf);

    while (grp && grp_y < y + h + listview->scrollpos) {
        // render title
        DdbListviewIter it = grp->head;
        listview->binding->ref (it);
        int grpheight = GROUP_TITLE_HEIGHT + grp->num_items * listview->rowheight;
        if (grp_y + GROUP_TITLE_HEIGHT >= y + listview->scrollpos && grp_y < y + h + listview->scrollpos) {
            ddb_listview_list_render_row_background (listview, NULL, idx & 1, 0, -listview->hscrollpos, grp_y - listview->scrollpos, listview->totalwidth, GROUP_TITLE_HEIGHT);
            listview->binding->draw_group_title (listview, listview->backbuf, it, -listview->hscrollpos, grp_y - listview->scrollpos, listview->totalwidth, GROUP_TITLE_HEIGHT);
        }
        for (int i = 0; i < grp->num_items; i++) {
            if (grp_y + GROUP_TITLE_HEIGHT + (i+1) * listview->rowheight >= y + listview->scrollpos && grp_y + GROUP_TITLE_HEIGHT + i * listview->rowheight< y + h + listview->scrollpos) {
                gdk_draw_rectangle (listview->backbuf, listview->list->style->bg_gc[GTK_STATE_NORMAL], TRUE, -listview->hscrollpos, grp_y + GROUP_TITLE_HEIGHT + i * listview->rowheight - listview->scrollpos, listview->totalwidth, listview->rowheight);
                ddb_listview_list_render_row_background (listview, it, (idx + 1 + i) & 1, (idx+i) == listview->binding->cursor () ? 1 : 0, -listview->hscrollpos, grp_y + GROUP_TITLE_HEIGHT + i * listview->rowheight - listview->scrollpos, listview->totalwidth, listview->rowheight);
                ddb_listview_list_render_row_foreground (listview, it, (idx + 1 + i) & 1, (idx+i) == listview->binding->cursor () ? 1 : 0, -listview->hscrollpos, grp_y + GROUP_TITLE_HEIGHT + i * listview->rowheight - listview->scrollpos, listview->totalwidth, listview->rowheight);
            }
            DdbListviewIter next = listview->binding->next (it);
            listview->binding->unref (it);
            it = next;
        }
        idx += grp->num_items + 1;
        grp_y += grpheight;
        grp = grp->next;
    }
    if (grp_y < y + h + listview->scrollpos) {
        int hh = y + h - (grp_y - listview->scrollpos);
//        gdk_draw_rectangle (listview->backbuf, listview->list->style->bg_gc[GTK_STATE_NORMAL], TRUE, x, grp_y - listview->scrollpos, w, hh);
        GtkWidget *treeview = theme_treeview;
        if (treeview->style->depth == -1) {
            return; // drawing was called too early
        }
        gtk_paint_flat_box (treeview->style, listview->backbuf, GTK_STATE_NORMAL, GTK_SHADOW_NONE, NULL, treeview, "cell_even_ruled", x, grp_y - listview->scrollpos, w, hh);
    }
    draw_end ();
}

gboolean
ddb_listview_list_expose_event               (GtkWidget       *widget,
        GdkEventExpose  *event,
        gpointer         user_data)
{
    DdbListview *ps = DDB_LISTVIEW (gtk_object_get_data (GTK_OBJECT (widget), "owner"));
    ddb_listview_list_expose (ps, event->area.x, event->area.y, event->area.width, event->area.height);
    return FALSE;
}

void
ddb_listview_list_expose (DdbListview *ps, int x, int y, int w, int h) {
    GtkWidget *widget = ps->list;
    if (widget->window) {
        draw_drawable (widget->window, widget->style->black_gc, ps->backbuf, x, y, x, y, w, h);
    }
}

gboolean
ddb_listview_vscroll_event               (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data)
{
    DdbListview *ps = DDB_LISTVIEW (gtk_object_get_data (GTK_OBJECT (widget), "owner"));
	GdkEventScroll *ev = (GdkEventScroll*)event;
    GtkWidget *range = ps->scrollbar;;
    GtkWidget *list = ps->list;
    // pass event to scrollbar
    int newscroll = gtk_range_get_value (GTK_RANGE (range));
    if (ev->direction == GDK_SCROLL_UP) {
        newscroll -= SCROLL_STEP;
    }
    else if (ev->direction == GDK_SCROLL_DOWN) {
        newscroll += SCROLL_STEP;
    }
    gtk_range_set_value (GTK_RANGE (range), newscroll);

    return FALSE;
}

void
ddb_listview_vscroll_value_changed            (GtkRange        *widget,
                                        gpointer         user_data)
{
    DdbListview *ps = DDB_LISTVIEW (gtk_object_get_data (GTK_OBJECT (widget), "owner"));
    int newscroll = gtk_range_get_value (GTK_RANGE (widget));
    if (newscroll != ps->scrollpos) {
        GtkWidget *widget = ps->list;
        int di = newscroll - ps->scrollpos;
        int d = abs (di);
        int height = ps->list->allocation.height;
        if (d < height) {
            if (di > 0) {
                // scroll down
                // copy scrolled part of buffer
                draw_drawable (ps->backbuf, widget->style->black_gc, ps->backbuf, 0, d, 0, 0, widget->allocation.width, widget->allocation.height-d);
                // redraw other part
                int start = height-d-1;
                ps->scrollpos = newscroll;
                ddb_listview_list_render (ps, 0, start, ps->list->allocation.width, height);
            }
            else {
                // scroll up
                // copy scrolled part of buffer
                draw_drawable (ps->backbuf, widget->style->black_gc, ps->backbuf, 0, 0, 0, d, widget->allocation.width, widget->allocation.height);
                // redraw other part
                ps->scrollpos = newscroll;
                ddb_listview_list_render (ps, 0, 0, ps->list->allocation.width, d+1);
            }
        }
        else {
            // scrolled more than view height, redraw everything
            ps->scrollpos = newscroll;
            ddb_listview_list_render (ps, 0, 0, widget->allocation.width, widget->allocation.height);
        }
        draw_drawable (widget->window, widget->style->black_gc, ps->backbuf, 0, 0, 0, 0, widget->allocation.width, widget->allocation.height);
    }
}

void
ddb_listview_hscroll_value_changed           (GtkRange        *widget,
                                        gpointer         user_data)
{
    DdbListview *pl = DDB_LISTVIEW (gtk_object_get_data (GTK_OBJECT (widget), "owner"));
    int newscroll = gtk_range_get_value (GTK_RANGE (widget));
    ddb_listview_list_set_hscroll (pl, newscroll);
}

gboolean
ddb_listview_list_drag_motion                (GtkWidget       *widget,
                                        GdkDragContext  *drag_context,
                                        gint             x,
                                        gint             y,
                                        guint            time,
                                        gpointer         user_data)
{
    DdbListview *pl = DDB_LISTVIEW (gtk_object_get_data (GTK_OBJECT (widget), "owner"));
    ddb_listview_list_track_dragdrop (pl, y);
    return FALSE;
}


gboolean
ddb_listview_list_drag_drop                  (GtkWidget       *widget,
                                        GdkDragContext  *drag_context,
                                        gint             x,
                                        gint             y,
                                        guint            time,
                                        gpointer         user_data)
{
    if (drag_context->targets) {
        GdkAtom target_type = GDK_POINTER_TO_ATOM (g_list_nth_data (drag_context->targets, TARGET_SAMEWIDGET));
        if (!target_type) {
            return FALSE;
        }
        gtk_drag_get_data (widget, drag_context, target_type, time);
        return TRUE;
    }
    return FALSE;
}


void
ddb_listview_list_drag_data_get              (GtkWidget       *widget,
                                        GdkDragContext  *drag_context,
                                        GtkSelectionData *selection_data,
                                        guint            target_type,
                                        guint            time,
                                        gpointer         user_data)
{
    DdbListview *ps = DDB_LISTVIEW (gtk_object_get_data (GTK_OBJECT (widget), "owner"));
    switch (target_type) {
    case TARGET_SAMEWIDGET:
        {
            // format as "STRING" consisting of array of pointers
            int nsel = ps->binding->sel_count ();
            if (!nsel) {
                break; // something wrong happened
            }
            uint32_t *ptr = malloc (nsel * sizeof (uint32_t));
            int idx = 0;
            int i = 0;
            DdbListviewIter it = ps->binding->head ();
            for (; it; idx++) {
                if (ps->binding->is_selected (it)) {
                    ptr[i] = idx;
                    i++;
                }
                DdbListviewIter next = ps->binding->next (it);
                ps->binding->unref (it);
                it = next;
            }
            gtk_selection_data_set (selection_data, selection_data->target, sizeof (uint32_t) * 8, (gchar *)ptr, nsel * sizeof (uint32_t));
            free (ptr);
        }
        break;
    default:
        g_assert_not_reached ();
    }
}


void
ddb_listview_list_drag_data_received         (GtkWidget       *widget,
                                        GdkDragContext  *drag_context,
                                        gint             x,
                                        gint             y,
                                        GtkSelectionData *data,
                                        guint            target_type,
                                        guint            time,
                                        gpointer         user_data)
{
    DdbListview *ps = DDB_LISTVIEW (gtk_object_get_data (GTK_OBJECT (widget), "owner"));
    gchar *ptr=(char*)data->data;
    if (target_type == 0) { // uris
        // this happens when dropped from file manager
        char *mem = malloc (data->length+1);
        memcpy (mem, ptr, data->length);
        mem[data->length] = 0;
        // we don't pass control structure, but there's only one drag-drop view currently
        DdbListviewGroup *grp;
        int grp_index;
        int sel;
        DdbListviewIter it = NULL;
        if (ddb_listview_list_pickpoint_y (ps, y + ps->scrollpos, &grp, &grp_index, &sel) != -1) {
            if (sel == -1) {
                sel = ps->binding->get_idx (grp->head);
            }
            it = ps->binding->get_for_idx (sel);
        }
        ps->binding->external_drag_n_drop (it, mem, data->length);
        if (it) {
            ps->binding->unref (it);
        }
    }
    else if (target_type == 1) {
        uint32_t *d= (uint32_t *)ptr;
        int length = data->length/4;
        DdbListviewIter drop_before = NULL;
        DdbListviewGroup *grp;
        int grp_index;
        int sel;
        if (ddb_listview_list_pickpoint_y (ps, y + ps->scrollpos, &grp, &grp_index, &sel) != -1) {
            if (sel == -1) {
                sel = ps->binding->get_idx (grp->head);
            }
            drop_before = ps->binding->get_for_idx (sel);
        }
        // find last selected
        while (drop_before && ps->binding->is_selected (drop_before)) {
            DdbListviewIter next = PL_NEXT(drop_before);
            UNREF (drop_before);
            drop_before = next;
        }
        ps->binding->drag_n_drop (drop_before, d, length);
    }
    gtk_drag_finish (drag_context, TRUE, FALSE, time);
}

gboolean
ddb_listview_list_drag_failed                (GtkWidget       *widget,
                                        GdkDragContext  *arg1,
                                        GtkDragResult    arg2,
                                        gpointer         user_data)
{
    return TRUE;
}


void
ddb_listview_list_drag_leave                 (GtkWidget       *widget,
                                        GdkDragContext  *drag_context,
                                        guint            time,
                                        gpointer         user_data)
{
    DdbListview *pl = DDB_LISTVIEW (gtk_object_get_data (GTK_OBJECT (widget), "owner"));
    ddb_listview_list_track_dragdrop (pl, -1);
}

// debug function for gdk_draw_drawable
static inline void
draw_drawable (GdkDrawable *window, GdkGC *gc, GdkDrawable *drawable, int x1, int y1, int x2, int y2, int w, int h) {
//    printf ("dd: %p %p %p %d %d %d %d %d %d\n", window, gc, drawable, x1, y1, x2, y2, w, h);
    gdk_draw_drawable (window, gc, drawable, x1, y1, x2, y2, w, h);
}

int
ddb_listview_get_vscroll_pos (DdbListview *listview) {
    return listview->scrollpos;
}

int
ddb_listview_get_hscroll_pos (DdbListview *listview) {
    return listview->hscrollpos;
}

#define MIN_COLUMN_WIDTH 16

static GdkCursor* cursor_sz;
static GdkCursor* cursor_drag;
#define COLHDR_ANIM_TIME 0.2f

#if 0
typedef struct {
    int c1;
    int c2;
    int x1, x2;
    int dx1, dx2;
    // animated values
    int ax1, ax2;
    timeline_t *timeline;
    int anim_active;
    DdbListview *pl;
} colhdr_animator_t;

static colhdr_animator_t colhdr_anim;

static gboolean
redraw_header (void *data) {
    colhdr_animator_t *anim = (colhdr_animator_t *)data;
    ddb_listview_header_render (anim->pl);
    ddb_listview_header_expose (anim->pl, 0, 0, anim->pl->header->allocation.width, anim->pl->header->allocation.height);
    return FALSE;
}

static int
colhdr_anim_cb (float _progress, int _last, void *_ctx) {
    colhdr_animator_t *anim = (colhdr_animator_t *)_ctx;
    anim->ax1 = anim->x1 + (float)(anim->dx1 - anim->x1) * _progress;
    anim->ax2 = anim->x2 + (float)(anim->dx2 - anim->x2) * _progress;
//    printf ("%f %d %d\n", _progress, anim->ax1, anim->ax2);
    g_idle_add (redraw_header, anim);
    if (_last) {
        anim->anim_active = 0;
    }
    return 0;
}

static void
colhdr_anim_swap (DdbListview *pl, int c1, int c2, int x1, int x2) {
    // interrupt previous anim
    if (!colhdr_anim.timeline) {
        colhdr_anim.timeline = timeline_create ();
    }
    colhdr_anim.pl = pl;

    colhdr_anim.c1 = c1;
    colhdr_anim.c2 = c2;

    // find c1 and c2 in column list and setup coords
    // note: columns are already swapped, so their coords must be reversed,
    // as if before swap
    DdbListviewColumn *c;
    int idx = 0;
    int x = 0;
    for (c = pl->columns; c; c = c->next, idx++) {
        if (idx == c1) {
            colhdr_anim.x1 = x1;
            colhdr_anim.dx2 = x;
        }
        else if (idx == c2) {
            colhdr_anim.x2 = x2;
            colhdr_anim.dx1 = x;
        }
        x += c->width;
    }
    colhdr_anim.anim_active = 1;
    timeline_stop (colhdr_anim.timeline, 0);
    timeline_init (colhdr_anim.timeline, COLHDR_ANIM_TIME, 100, colhdr_anim_cb, &colhdr_anim);
    timeline_start (colhdr_anim.timeline);
}
#endif

void
ddb_listview_list_setup_vscroll (DdbListview *ps) {
    GtkWidget *list = ps->list;
    GtkWidget *scroll = ps->scrollbar;
    if (ps->fullheight <= ps->list->allocation.height) {
        gtk_widget_hide (scroll);
        ps->scrollpos = 0;
    }
    else {
        int h = list->allocation.height;
        int vheight = ps->fullheight;;
        GtkAdjustment *adj = (GtkAdjustment*)gtk_adjustment_new (gtk_range_get_value (GTK_RANGE (scroll)), 0, vheight, SCROLL_STEP, h/2, h);
        gtk_range_set_adjustment (GTK_RANGE (scroll), adj);
        gtk_widget_show (scroll);
    }
}

void
ddb_listview_list_setup_hscroll (DdbListview *ps) {
    GtkWidget *list = ps->list;
    int w = list->allocation.width;
    int size = 0;
    DdbListviewColumn *c;
    for (c = ps->columns; c; c = c->next) {
        size += c->width;
    }
    ps->totalwidth = size;
    if (ps->totalwidth < ps->list->allocation.width) {
        ps->totalwidth = ps->list->allocation.width;
    }
    if (w >= size) {
        size = 0;
    }
    GtkWidget *scroll = ps->hscrollbar;
    if (ps->hscrollpos >= size-w) {
        int n = size-w-1;
        ps->hscrollpos = max (0, n);
        gtk_range_set_value (GTK_RANGE (scroll), ps->hscrollpos);
    }
    if (size == 0) {
        gtk_widget_hide (scroll);
    }
    else {
        GtkAdjustment *adj = (GtkAdjustment*)gtk_adjustment_new (gtk_range_get_value (GTK_RANGE (scroll)), 0, size, 1, w, w);
        gtk_range_set_adjustment (GTK_RANGE (scroll), adj);
        gtk_widget_show (scroll);
    }
}

// returns -1 if row not found
int
ddb_listview_list_get_drawinfo (DdbListview *listview, int row, int *even, int *cursor, int *x, int *y, int *w, int *h) {
    DdbListviewGroup *grp = listview->groups;
    int idx = 0;
    int idx2 = 0;
    *y = -listview->scrollpos;
    while (grp) {
        int grpheight = GROUP_TITLE_HEIGHT + grp->num_items * listview->rowheight;
        if (idx <= row && idx + grp->num_items > row) {
            // found
            int idx_in_group = row - idx;
            *even = (idx2 + 1 + idx_in_group) & 1;
            *cursor = (row == listview->binding->cursor ()) ? 1 : 0;
            *x = -listview->hscrollpos;
            *y += GROUP_TITLE_HEIGHT + (row - idx) * listview->rowheight;
            *w = listview->totalwidth;
            *h = listview->rowheight;
            return 0;
        }
        *y += grpheight;
        idx += grp->num_items;
        idx2 += grp->num_items + 1;
        grp = grp->next;
    }
    return -1;
}

void
ddb_listview_list_render_row (DdbListview *listview, int row, DdbListviewIter it, int expose) {
    int even;
    int cursor;
    int x, y, w, h;
    if (ddb_listview_list_get_drawinfo (listview, row, &even, &cursor, &x, &y, &w, &h) == -1) {
        return;
    }

    draw_begin ((uintptr_t)listview->backbuf);
    ddb_listview_list_render_row_background (listview, it, even, cursor, x, y, w, h);
	if (it) {
        ddb_listview_list_render_row_foreground (listview, it, even, cursor, x, y, w, h);
    }
    draw_end ();
    if (expose) {
        draw_drawable (listview->list->window, listview->list->style->black_gc, listview->backbuf, 0, y, 0, y, listview->list->allocation.width, h);
    }
}

void
ddb_listview_draw_row (DdbListview *listview, int row, DdbListviewIter it) {
    ddb_listview_list_render_row (listview, row, it, 1);
}

// coords passed are window-relative
void
ddb_listview_list_render_row_background (DdbListview *ps, DdbListviewIter it, int even, int cursor, int x, int y, int w, int h) {
	// draw background
	GtkWidget *treeview = theme_treeview;
	if (treeview->style->depth == -1) {
        return; // drawing was called too early
    }
    GTK_OBJECT_FLAGS (treeview) |= GTK_HAS_FOCUS;
    if (it && ps->binding->is_selected(it)) {
        // draw background for selection -- workaround for New Wave theme (translucency)
        gtk_paint_flat_box (treeview->style, ps->backbuf, GTK_STATE_NORMAL, GTK_SHADOW_NONE, NULL, treeview, even ? "cell_even_ruled" : "cell_odd_ruled", x, y, w, h);
    }
    gtk_paint_flat_box (treeview->style, ps->backbuf, (it && ps->binding->is_selected(it)) ? GTK_STATE_SELECTED : GTK_STATE_NORMAL, GTK_SHADOW_NONE, NULL, treeview, even ? "cell_even_ruled" : "cell_odd_ruled", x, y, w, h);
	if (cursor) {
        // not all gtk engines/themes render focus rectangle in treeviews
        // but we want it anyway
        gdk_draw_rectangle (ps->backbuf, treeview->style->fg_gc[GTK_STATE_NORMAL], FALSE, x, y, w-1, h-1);
    }
}

void
ddb_listview_list_render_row_foreground (DdbListview *ps, DdbListviewIter it, int even, int cursor, int x, int y, int w, int h) {
	int width, height;
	draw_get_canvas_size ((uintptr_t)ps->backbuf, &width, &height);
	if (it && ps->binding->is_selected (it)) {
        GdkColor *clr = &theme_treeview->style->fg[GTK_STATE_SELECTED];
        float rgb[3] = { clr->red/65535.f, clr->green/65535.f, clr->blue/65535.f };
        draw_set_fg_color (rgb);
    }
    else {
        GdkColor *clr = &theme_treeview->style->fg[GTK_STATE_NORMAL];
        float rgb[3] = { clr->red/65535.f, clr->green/65535.f, clr->blue/65535.f };
        draw_set_fg_color (rgb);
    }
    DdbListviewColumn *c;
    int cidx = 0;
    for (c = ps->columns; c; c = c->next, cidx++) {
        int cw = c->width;
        ps->binding->draw_column_data (ps, ps->backbuf, it, cidx, x, y, cw, h);
        x += cw;
    }
}


void
ddb_listview_header_expose (DdbListview *ps, int x, int y, int w, int h) {
    GtkWidget *widget = ps->header;
	draw_drawable (widget->window, widget->style->black_gc, ps->backbuf_header, x, y, x, y, w, h);
}

void
ddb_listview_select_single (DdbListview *ps, int sel) {
    int idx=0;
    DdbListviewIter it = ps->binding->head ();
    for (; it; idx++) {
        if (idx == sel) {
            if (!ps->binding->is_selected (it)) {
                ps->binding->select (it, 1);
                ddb_listview_draw_row (ps, idx, it);
                ps->binding->selection_changed (it, idx);
            }
        }
        else if (ps->binding->is_selected (it)) {
            ps->binding->select (it, 0);
            ddb_listview_draw_row (ps, idx, it);
            ps->binding->selection_changed (it, idx);
        }
        DdbListviewIter next = PL_NEXT (it);
        UNREF (it);
        it = next;
    }
    UNREF (it);
}

// {{{ expected behaviour for mouse1 without modifiers:
//   {{{ [+] if clicked unselected item:
//       unselect all
//       select clicked item
//       deadbeef->pl_get_cursor (ps->iterator) = clicked
//       redraw
//       start 'area selection' mode
//   }}}
//   {{{ [+] if clicked selected item:
//       deadbeef->pl_get_cursor (ps->iterator) = clicked
//       redraw
//       wait until next release or motion event, whichever is 1st
//       if release is 1st:
//           unselect all except clicked, redraw
//       else if motion is 1st:
//           enter drag-drop mode
//   }}}
// }}}
void
ddb_listview_list_mouse1_pressed (DdbListview *ps, int state, int ex, int ey, double time) {
    // cursor must be set here, but selection must be handled in keyrelease
    int cnt = ps->binding->count ();
    if (cnt == 0) {
        return;
    }
    // remember mouse coords for doubleclick detection
    ps->lastpos[0] = ex;
    ps->lastpos[1] = ey;
    // select item
    DdbListviewGroup *grp;
    int grp_index;
    int sel;
    if (ddb_listview_list_pickpoint_y (ps, ey + ps->scrollpos, &grp, &grp_index, &sel) == -1) {
        return;
    }

    int cursor = ps->binding->cursor ();
    if (time - ps->clicktime < 0.5
            && fabs(ps->lastpos[0] - ex) < 3
            && fabs(ps->lastpos[1] - ey) < 3) {
        // doubleclick - play this item
        if (sel != -1 && cursor != -1) {
            int idx = cursor;
            DdbListviewIter it = ps->binding->get_for_idx (idx);
            if (ps->binding->handle_doubleclick && it) {
                ps->binding->handle_doubleclick (ps, it, idx);
            }
            if (it) {
                ps->binding->unref (it);
            }
            return;
        }

        // prevent next click to trigger doubleclick
        ps->clicktime = time-1;
    }
    else {
        ps->clicktime = time;
    }

//    if (sel == -1) {
//        sel = ps->binding->count () - 1;
//    }
    int prev = cursor;
    if (sel != -1) {
        ps->binding->set_cursor (sel);
        ps->shift_sel_anchor = ps->binding->cursor ();
    }
    // handle multiple selection
    if (!(state & (GDK_CONTROL_MASK|GDK_SHIFT_MASK)))
    {
        if (sel == -1 && grp) {
            DdbListviewIter it;
            int idx = 0;
            int cnt = -1;
            for (it = ps->binding->head (); it; idx++) {
                if (it == grp->head) {
                    cnt = grp->num_items;
                }
                if (cnt > 0) {
                    if (!ps->binding->is_selected (it)) {
                        ps->binding->select (it, 1);
                        ddb_listview_draw_row (ps, idx, it);
                        ps->binding->selection_changed (it, idx);
                    }
                    cnt--;
                }
                else {
                    if (ps->binding->is_selected (it)) {
                        ps->binding->select (it, 0);
                        ddb_listview_draw_row (ps, idx, it);
                        ps->binding->selection_changed (it, idx);
                    }
                }
                DdbListviewIter next = ps->binding->next (it);
                ps->binding->unref (it);
                it = next;
            }
        }
        else {
            DdbListviewIter it = ps->binding->get_for_idx (sel);
            if (!it || !ps->binding->is_selected (it)) {
                // reset selection, and set it to single item
                ddb_listview_select_single (ps, sel);
                ps->areaselect = 1;
                ps->areaselect_x = ex;
                ps->areaselect_y = ey;
                ps->areaselect_dx = -1;
                ps->areaselect_dy = -1;
                ps->shift_sel_anchor = ps->binding->cursor ();
            }
            else {
                ps->dragwait = 1;
                DdbListviewIter item = ps->binding->get_for_idx (prev);
                ddb_listview_draw_row (ps, prev, item);
                UNREF (item);
                int cursor = ps->binding->cursor ();
                if (cursor != prev) {
                    DdbListviewIter item = ps->binding->get_for_idx (cursor);
                    ddb_listview_draw_row (ps, cursor, item);
                    UNREF (item);
                }
            }
            UNREF (it);
        }
    }
    else if (state & GDK_CONTROL_MASK) {
        // toggle selection
        if (sel != -1) {
            DdbListviewIter it = ps->binding->get_for_idx (sel);
            if (it) {
                ps->binding->select (it, 1 - ps->binding->is_selected (it));
                ddb_listview_draw_row (ps, sel, it);
                ps->binding->selection_changed (it, sel);
                UNREF (it);
            }
        }
    }
    else if (state & GDK_SHIFT_MASK) {
        // select range
        int cursor = sel;//ps->binding->cursor ();
        if (cursor == -1) {
            // find group
            DdbListviewGroup *g = ps->groups;
            int idx = 0;
            while (g) {
                if (g == grp) {
                    cursor = idx - 1;
                    break;
                }
                idx += g->num_items;
                g = g->next;
            }
        }
        int start = min (prev, cursor);
        int end = max (prev, cursor);
        int idx = 0;
        for (DdbListviewIter it = ps->binding->head (); it; idx++) {
            if (idx >= start && idx <= end) {
                if (!ps->binding->is_selected (it)) {
                    ps->binding->select (it, 1);
                    ddb_listview_draw_row (ps, idx, it);
                    ps->binding->selection_changed (it, idx);
                }
            }
            else {
                if (ps->binding->is_selected (it)) {
                    ps->binding->select (it, 0);
                    ddb_listview_draw_row (ps, idx, it);
                    ps->binding->selection_changed (it, idx);
                }
            }
            DdbListviewIter next = PL_NEXT (it);
            UNREF (it);
            it = next;
        }
    }
    cursor = ps->binding->cursor ();
    if (cursor != -1 && sel == -1) {
        DdbListviewIter it = ps->binding->get_for_idx (cursor);
        ddb_listview_draw_row (ps, cursor, it);
        UNREF (it);
    }
    if (prev != -1 && prev != cursor) {
        DdbListviewIter it = ps->binding->get_for_idx (prev);
        ddb_listview_draw_row (ps, prev, it);
        UNREF (it);
    }

}

void
ddb_listview_list_mouse1_released (DdbListview *ps, int state, int ex, int ey, double time) {
    if (ps->dragwait) {
        ps->dragwait = 0;
#if 0
        int y = ey/ps->rowheight + ps->scrollpos;
        ddb_listview_select_single (ps, y);
#endif
    }
    else if (ps->areaselect) {
        ps->scroll_direction = 0;
        ps->scroll_pointer_y = -1;
        ps->areaselect = 0;
    }
}

#if 0
void
ddb_listview_list_dbg_draw_areasel (GtkWidget *widget, int x, int y) {
    // erase previous rect using 4 blits from ps->backbuffer
    if (areaselect_dx != -1) {
        int sx = min (areaselect_x, areaselect_dx);
        int sy = min (areaselect_y, areaselect_dy);
        int dx = max (areaselect_x, areaselect_dx);
        int dy = max (areaselect_y, areaselect_dy);
        int w = dx - sx + 1;
        int h = dy - sy + 1;
        //draw_drawable (widget->window, widget->style->black_gc, ps->backbuf, sx, sy, sx, sy, dx - sx + 1, dy - sy + 1);
        draw_drawable (widget->window, widget->style->black_gc, ps->backbuf, sx, sy, sx, sy, w, 1);
        draw_drawable (widget->window, widget->style->black_gc, ps->backbuf, sx, sy, sx, sy, 1, h);
        draw_drawable (widget->window, widget->style->black_gc, ps->backbuf, sx, sy + h - 1, sx, sy + h - 1, w, 1);
        draw_drawable (widget->window, widget->style->black_gc, ps->backbuf, sx + w - 1, sy, sx + w - 1, sy, 1, h);
    }
    areaselect_dx = x;
    areaselect_dy = y;
	cairo_t *cr;
	cr = gdk_cairo_create (widget->window);
	if (!cr) {
		return;
	}
    cairo_set_antialias (cr, CAIRO_ANTIALIAS_NONE);
    cairo_set_line_width (cr, 1);
    int sx = min (areaselect_x, x);
    int sy = min (areaselect_y, y);
    int dx = max (areaselect_x, x);
    int dy = max (areaselect_y, y);
    cairo_rectangle (cr, sx, sy, dx-sx, dy-sy);
    cairo_stroke (cr);
    cairo_destroy (cr);
}
#endif

static gboolean
ddb_listview_list_scroll_cb (gpointer data) {
    DdbListview *ps = (DdbListview *)data;
    ps->scroll_active = 1;
    struct timeval tm;
    gettimeofday (&tm, NULL);
    float dt = tm.tv_sec - ps->tm_prevscroll.tv_sec + (tm.tv_usec - ps->tm_prevscroll.tv_usec) / 1000000.0;
    if (dt < ps->scroll_sleep_time) {
        return TRUE;
    }
    memcpy (&ps->tm_prevscroll, &tm, sizeof (tm));
    if (ps->scroll_pointer_y == -1) {
        ps->scroll_active = 0;
        return FALSE;
    }
    if (ps->scroll_direction == 0) {
        ps->scroll_active = 0;
        return FALSE;
    }
    int sc = ps->scrollpos + (ps->scroll_direction * 100 * dt);
    if (sc < 0) {
        ps->scroll_active = 0;
        return FALSE;
    }
//    trace ("scroll to %d speed %f\n", sc, ps->scroll_direction);
    gtk_range_set_value (GTK_RANGE (ps->scrollbar), sc);
    if (ps->scroll_mode == 0) {
        GdkEventMotion ev;
        ev.y = ps->scroll_pointer_y;
        ddb_listview_list_mousemove (ps, &ev);
    }
    else if (ps->scroll_mode == 1) {
        ddb_listview_list_track_dragdrop (ps, ps->scroll_pointer_y);
    }
    if (ps->scroll_direction < 0) {
        ps->scroll_direction -= (10 * dt);
        if (ps->scroll_direction < -30) {
            ps->scroll_direction = -30;
        }
    }
    else {
        ps->scroll_direction += (10 * dt);
        if (ps->scroll_direction > 30) {
            ps->scroll_direction = 30;
        }
    }
    return TRUE;
}

void
ddb_listview_list_mousemove (DdbListview *ps, GdkEventMotion *event) {
    if (ps->dragwait) {
        GtkWidget *widget = ps->list;
        if (gtk_drag_check_threshold (widget, ps->lastpos[0], event->x, ps->lastpos[1], event->y)) {
            ps->dragwait = 0;
            GtkTargetEntry entry = {
                .target = "STRING",
                .flags = GTK_TARGET_SAME_WIDGET,
                .info = TARGET_SAMEWIDGET
            };
            GtkTargetList *lst = gtk_target_list_new (&entry, 1);
            gtk_drag_begin (widget, lst, GDK_ACTION_MOVE, TARGET_SAMEWIDGET, (GdkEvent *)event);
        }
    }
    else if (ps->areaselect) {
        DdbListviewGroup *grp;
        int grp_index;
        int sel;
        if (ddb_listview_list_pickpoint_y (ps, event->y + ps->scrollpos, &grp, &grp_index, &sel) == -1) {
            return; // nothing was hit
        }
        {
            // select range of items
            int y = sel;
            int idx = 0;
            if (y == -1) {
                // find group
                DdbListviewGroup *g = ps->groups;
                while (g) {
                    if (g == grp) {
                        y = idx - 1;
                        break;
                    }
                    idx += g->num_items;
                    g = g->next;
                }
            }
            int start = min (y, ps->shift_sel_anchor);
            int end = max (y, ps->shift_sel_anchor);

            idx=0;
            DdbListviewIter it;
            for (it = ps->binding->head (); it; idx++) {
                if (idx >= start && idx <= end) {
                    if (!ps->binding->is_selected (it)) {
                        ps->binding->select (it, 1);
                        ddb_listview_draw_row (ps, idx, it);
                        ps->binding->selection_changed (it, idx);
                    }
                }
                else if (ps->binding->is_selected (it)) {
                    ps->binding->select (it, 0);
                    ddb_listview_draw_row (ps, idx, it);
                    ps->binding->selection_changed (it, idx);
                }
                DdbListviewIter next = PL_NEXT(it);
                UNREF (it);
                it = next;
            }
            UNREF (it);
        }

        if (event->y < 10) {
            ps->scroll_mode = 0;
            ps->scroll_pointer_y = event->y;
            // start scrolling up
            if (!ps->scroll_active) {
                ps->scroll_direction = -1;
                ps->scroll_sleep_time = AUTOSCROLL_UPDATE_FREQ;
                gettimeofday (&ps->tm_prevscroll, NULL);
                g_idle_add (ddb_listview_list_scroll_cb, ps);
            }
        }
        else if (event->y > ps->list->allocation.height-10) {
            ps->scroll_mode = 0;
            ps->scroll_pointer_y = event->y;
            // start scrolling up
            if (!ps->scroll_active) {
                ps->scroll_direction = 1;
                ps->scroll_sleep_time = AUTOSCROLL_UPDATE_FREQ;
                gettimeofday (&ps->tm_prevscroll, NULL);
                g_idle_add (ddb_listview_list_scroll_cb, ps);
            }
        }
        else {
            ps->scroll_direction = 0;
            ps->scroll_pointer_y = -1;
        }
        // debug only
        // ddb_listview_list_dbg_draw_areasel (widget, event->x, event->y);
    }
}

void
ddb_listview_list_set_hscroll (DdbListview *ps, int newscroll) {
    if (newscroll != ps->hscrollpos) {
        ps->hscrollpos = newscroll;
        GtkWidget *widget = ps->list;
        ddb_listview_header_render (ps);
        ddb_listview_header_expose (ps, 0, 0, ps->header->allocation.width, ps->header->allocation.height);
        ddb_listview_list_render (ps, 0, 0, widget->allocation.width, widget->allocation.height);
        draw_drawable (widget->window, widget->style->black_gc, ps->backbuf, 0, 0, 0, 0, widget->allocation.width, widget->allocation.height);
    }
}

int
ddb_listview_handle_keypress (DdbListview *ps, int keyval, int state) {
    int prev = ps->binding->cursor ();
    int cursor = prev;
    if (keyval == GDK_Down) {
        if (cursor < ps->binding->count () - 1) {
            cursor++;
        }
    }
    else if (keyval == GDK_Up) {
        if (cursor > 0) {
            cursor--;
        }
        else if (cursor < 0 && ps->binding->count () > 0) {
            cursor = 0;
        }
    }
    else if (keyval == GDK_Page_Down) {
        if (cursor < ps->binding->count () - 1) {
            cursor += 10;
            if (cursor >= ps->binding->count ()) {
                cursor = ps->binding->count () - 1;
            }
        }
    }
    else if (keyval == GDK_Page_Up) {
        if (cursor > 0) {
            cursor -= 10;
            if (cursor < 0) {
                cursor = 0;
            }
        }
    }
    else if (keyval == GDK_End) {
        cursor = ps->binding->count () - 1;
    }
    else if (keyval == GDK_Home) {
        cursor = 0;
    }
    else if (keyval == GDK_Delete) {
        ps->binding->delete_selected ();
        cursor = ps->binding->cursor ();
    }
    else {
        return 0 ;
    }
    if (state & GDK_SHIFT_MASK) {
        if (cursor != prev) {
            int newscroll = ps->scrollpos;
            int cursor_scroll = ddb_listview_get_row_pos (ps, cursor);
            if (cursor_scroll < ps->scrollpos) {
                newscroll = cursor_scroll;
            }
            else if (cursor_scroll >= ps->scrollpos + ps->list->allocation.height) {
                newscroll = cursor_scroll - ps->list->allocation.height + 1;
                if (newscroll < 0) {
                    newscroll = 0;
                }
            }
            if (ps->scrollpos != newscroll) {
                GtkWidget *range = ps->scrollbar;
                gtk_range_set_value (GTK_RANGE (range), newscroll);
            }

            ps->binding->set_cursor (cursor);
            // select all between shift_sel_anchor and deadbeef->pl_get_cursor (ps->iterator)
            int start = min (cursor, ps->shift_sel_anchor);
            int end = max (cursor, ps->shift_sel_anchor);
            int idx=0;
            DdbListviewIter it;
            for (it = ps->binding->head (); it; idx++) {
                if (idx >= start && idx <= end) {
                    ps->binding->select (it, 1);
                    ddb_listview_draw_row (ps, idx, it);
                    ps->binding->selection_changed (it, idx);
                }
                else if (ps->binding->is_selected (it))
                {
                    ps->binding->select (it, 0);
                    ddb_listview_draw_row (ps, idx, it);
                    ps->binding->selection_changed (it, idx);
                }
                DdbListviewIter next = PL_NEXT(it);
                UNREF (it);
                it = next;
            }
            UNREF (it);
        }
    }
    else {
        ps->shift_sel_anchor = cursor;
        ddb_listview_set_cursor (ps, cursor);
    }
    return 1;
}

void
ddb_listview_list_track_dragdrop (DdbListview *ps, int y) {
    GtkWidget *widget = ps->list;
    if (ps->drag_motion_y != -1) {
        // erase previous track
        draw_drawable (widget->window, widget->style->black_gc, ps->backbuf, 0, ps->drag_motion_y-3, 0, ps->drag_motion_y-3, widget->allocation.width, 7);

    }
    if (y == -1) {
        ps->drag_motion_y = -1;
        return;
    }
    DdbListviewGroup *grp;
    int grp_index;
    int sel;
    if (ddb_listview_list_pickpoint_y (ps, y + ps->scrollpos, &grp, &grp_index, &sel) == -1) {
        if (ps->binding->count () == 0) {
            ps->drag_motion_y = 0;
        }
        else {
            ps->drag_motion_y = ddb_listview_get_row_pos (ps, ps->binding->count ()-1) - ps->scrollpos + ps->rowheight;
        }
    }
    else {
        if (sel == -1) {
            sel = ps->binding->get_idx (grp->head);
        }
        ps->drag_motion_y = ddb_listview_get_row_pos (ps, sel) - ps->scrollpos;
    }

    draw_begin ((uintptr_t)widget->window);
    GtkStyle *style = gtk_widget_get_style (GTK_WIDGET (ps));
    float clr[3] = { style->fg[GTK_STATE_NORMAL].red, style->fg[GTK_STATE_NORMAL].green, style->fg[GTK_STATE_NORMAL].blue };
    draw_set_fg_color (clr);

    draw_rect (0, ps->drag_motion_y-1, widget->allocation.width, 3, 1);
    draw_rect (0, ps->drag_motion_y-3, 3, 7, 1);
    draw_rect (widget->allocation.width-3, ps->drag_motion_y-3, 3, 7, 1);
    draw_end ();
    if (y < 10) {
        ps->scroll_pointer_y = y;
        ps->scroll_mode = 1;
        // start scrolling up
        if (!ps->scroll_active) {
            ps->scroll_direction = -1;
            ps->scroll_sleep_time = AUTOSCROLL_UPDATE_FREQ;
            gettimeofday (&ps->tm_prevscroll, NULL);
            g_idle_add (ddb_listview_list_scroll_cb, ps);
        }
    }
    else if (y > ps->list->allocation.height-10) {
        ps->scroll_mode = 1;
        ps->scroll_pointer_y = y;
        // start scrolling up
        if (!ps->scroll_active) {
            ps->scroll_direction = 1;
            ps->scroll_sleep_time = AUTOSCROLL_UPDATE_FREQ;
            gettimeofday (&ps->tm_prevscroll, NULL);
            g_idle_add (ddb_listview_list_scroll_cb, ps);
        }
    }
    else {
        ps->scroll_direction = 0;
        ps->scroll_pointer_y = -1;
    }
}

void
ddb_listview_list_drag_end                   (GtkWidget       *widget,
                                        GdkDragContext  *drag_context,
                                        gpointer         user_data)
{
    DdbListview *ps = DDB_LISTVIEW (gtk_object_get_data (GTK_OBJECT (widget), "owner"));
    ddb_listview_refresh (ps, DDB_REFRESH_LIST|DDB_EXPOSE_LIST);
    ps->scroll_direction = 0;
    ps->scroll_pointer_y = -1;
}

void
ddb_listview_header_render (DdbListview *ps) {
    GtkWidget *widget = ps->header;
    int x = -ps->hscrollpos;
    int w = 100;
    int h = widget->allocation.height;
    const char *detail = "button";

    // fill background
    gtk_paint_box (widget->style, ps->backbuf_header, GTK_STATE_NORMAL, GTK_SHADOW_OUT, NULL, widget, detail, -10, -10, widget->allocation.width+20, widget->allocation.height+20);
    gdk_draw_line (ps->backbuf_header, widget->style->mid_gc[GTK_STATE_NORMAL], 0, widget->allocation.height-1, widget->allocation.width, widget->allocation.height-1);
    draw_begin ((uintptr_t)ps->backbuf_header);
    x = -ps->hscrollpos;
    DdbListviewColumn *c;
    int need_draw_moving = 0;
    int idx = 0;
    for (c = ps->columns; c; c = c->next, idx++) {
        w = c->width;
        int xx = x;
#if 0
        if (colhdr_anim.anim_active) {
            if (idx == colhdr_anim.c2) {
                xx = colhdr_anim.ax1;
            }
            else if (idx == colhdr_anim.c1) {
                xx = colhdr_anim.ax2;
            }
        }
#endif
        if (ps->header_dragging < 0 || idx != ps->header_dragging) {
            if (xx >= widget->allocation.width) {
                continue;
            }
            int arrow_sz = 10;
            int sort = c->sort_order;
            if (w > 0) {
                gtk_paint_vline (widget->style, ps->backbuf_header, GTK_STATE_NORMAL, NULL, NULL, NULL, 2, h-4, xx+w - 2);
                GdkColor *gdkfg = &widget->style->fg[0];
                float fg[3] = {(float)gdkfg->red/0xffff, (float)gdkfg->green/0xffff, (float)gdkfg->blue/0xffff};
                draw_set_fg_color (fg);
                int ww = w-10;
                if (sort) {
                    ww -= arrow_sz;
                    if (ww < 0) {
                        ww = 0;
                    }
                }
                draw_text (xx + 5, h/2-draw_get_font_size()/2, ww, 0, c->title);
            }
            if (sort) {
                int dir = sort == 1 ? GTK_ARROW_DOWN : GTK_ARROW_UP;
                gtk_paint_arrow (widget->style, ps->backbuf_header, GTK_STATE_NORMAL, GTK_SHADOW_NONE, NULL, widget, NULL, dir, TRUE, xx + w-arrow_sz-5, widget->allocation.height/2-arrow_sz/2, arrow_sz, arrow_sz);
            }
        }
        else {
            need_draw_moving = 1;
        }
        x += w;
    }
    if (need_draw_moving) {
        x = -ps->hscrollpos;
        idx = 0;
        for (c = ps->columns; c; c = c->next, idx++) {
            w = c->width;
            if (idx == ps->header_dragging) {
#if 0
                if (colhdr_anim.anim_active) {
                    if (idx == colhdr_anim.c2) {
                        x = colhdr_anim.ax1;
                    }
                    else if (idx == colhdr_anim.c1) {
                        x = colhdr_anim.ax2;
                    }
                }
#endif
                // draw empty slot
                if (x < widget->allocation.width) {
                    gtk_paint_box (widget->style, ps->backbuf_header, GTK_STATE_ACTIVE, GTK_SHADOW_ETCHED_IN, NULL, widget, "button", x, 0, w, h);
                }
                x = ps->col_movepos;
                if (x >= widget->allocation.width) {
                    break;
                }
                if (w > 0) {
                    gtk_paint_box (widget->style, ps->backbuf_header, GTK_STATE_SELECTED, GTK_SHADOW_OUT, NULL, widget, "button", x, 0, w, h);
                    GdkColor *gdkfg = &widget->style->fg[GTK_STATE_SELECTED];
                    float fg[3] = {(float)gdkfg->red/0xffff, (float)gdkfg->green/0xffff, (float)gdkfg->blue/0xffff};
                    draw_set_fg_color (fg);
                    draw_text (x + 5, h/2-draw_get_font_size()/2, c->width-10, 0, c->title);
                }
                break;
            }
            x += w;
        }
    }
    draw_end ();
}

gboolean
ddb_listview_header_expose_event                 (GtkWidget       *widget,
                                        GdkEventExpose  *event,
                                        gpointer         user_data)
{
    DdbListview *ps = DDB_LISTVIEW (gtk_object_get_data (GTK_OBJECT (widget), "owner"));
    ddb_listview_header_expose (ps, event->area.x, event->area.y, event->area.width, event->area.height);
    return FALSE;
}


gboolean
ddb_listview_header_configure_event              (GtkWidget       *widget,
                                        GdkEventConfigure *event,
                                        gpointer         user_data)
{
    DdbListview *ps = DDB_LISTVIEW (gtk_object_get_data (GTK_OBJECT (widget), "owner"));
    if (ps->backbuf_header) {
        g_object_unref (ps->backbuf_header);
        ps->backbuf_header = NULL;
    }
    ps->backbuf_header = gdk_pixmap_new (widget->window, widget->allocation.width, widget->allocation.height, -1);
    ddb_listview_header_render (ps);
    return FALSE;
}


void
ddb_listview_header_realize                      (GtkWidget       *widget,
                                        gpointer         user_data)
{
    // create cursor for sizing headers
    int h = draw_get_font_size ();
    gtk_widget_set_size_request (widget, -1, h + 10);
    cursor_sz = gdk_cursor_new (GDK_SB_H_DOUBLE_ARROW);
    cursor_drag = gdk_cursor_new (GDK_FLEUR);
}

gboolean
ddb_listview_header_motion_notify_event          (GtkWidget       *widget,
                                        GdkEventMotion  *event,
                                        gpointer         user_data)
{
    DdbListview *ps = DDB_LISTVIEW (gtk_object_get_data (GTK_OBJECT (widget), "owner"));
    int ev_x, ev_y;
    GdkModifierType ev_state;

    if (event->is_hint)
        gdk_window_get_pointer (event->window, &ev_x, &ev_y, &ev_state);
    else
    {
        ev_x = event->x;
        ev_y = event->y;
        ev_state = event->state;
    }


    if ((ev_state & GDK_BUTTON1_MASK) && ps->header_prepare) {
        if (gtk_drag_check_threshold (widget, ev_x, ps->prev_header_x, 0, 0)) {
            ps->header_prepare = 0;
        }
    }
    if (!ps->header_prepare && ps->header_dragging >= 0) {
        gdk_window_set_cursor (widget->window, cursor_drag);
        DdbListviewColumn *c;
        int i;
        for (i = 0, c = ps->columns; i < ps->header_dragging && c; c = c->next, i++);
        ps->col_movepos = ev_x - ps->header_dragpt[0];

        // find closest column to the left
        int inspos = -1;
        DdbListviewColumn *cc;
        int x = 0;
        int idx = 0;
        int x1 = -1, x2 = -1;
        for (cc = ps->columns; cc; cc = cc->next, idx++) {
            if (x < ps->col_movepos && x + c->width > ps->col_movepos) {
                inspos = idx;
                x1 = x;
            }
            else if (idx == ps->header_dragging) {
                x2 = x;
            }
            x += cc->width;
        }
        if (inspos >= 0 && inspos != ps->header_dragging) {
            ddb_listview_column_move (ps, c, inspos);
//            ps->binding->col_move (c, inspos);
            ps->header_dragging = inspos;
//            colhdr_anim_swap (ps, c1, c2, x1, x2);
            // force redraw of everything
//            ddb_listview_list_setup_hscroll (ps);
            ddb_listview_list_render (ps, 0, 0, ps->list->allocation.width, ps->list->allocation.height);
            ddb_listview_list_expose (ps, 0, 0, ps->list->allocation.width, ps->list->allocation.height);
        }
        else {
            // only redraw that if not animating
            ddb_listview_header_render (ps);
            ddb_listview_header_expose (ps, 0, 0, ps->header->allocation.width, ps->header->allocation.height);
        }
    }
    else if (ps->header_sizing >= 0) {
        ps->last_header_motion_ev = event->time;
        ps->prev_header_x = ev_x;
        gdk_window_set_cursor (widget->window, cursor_sz);
        // get column start pos
        int x = -ps->hscrollpos;
        int i = 0;
        DdbListviewColumn *c;
        for (c = ps->columns; c && i < ps->header_sizing; c = c->next, i++) {
            x += c->width;
        }

        int newx = ev_x > x + MIN_COLUMN_WIDTH ? ev_x : x + MIN_COLUMN_WIDTH;
        c->width = newx-x;
        ddb_listview_list_setup_hscroll (ps);
        ddb_listview_header_render (ps);
        ddb_listview_header_expose (ps, 0, 0, ps->header->allocation.width, ps->header->allocation.height);
        ddb_listview_list_render (ps, 0, 0, ps->list->allocation.width, ps->list->allocation.height);
        ddb_listview_list_expose (ps, 0, 0, ps->list->allocation.width, ps->list->allocation.height);
    }
    else {
        int x = -ps->hscrollpos;
        DdbListviewColumn *c;
        for (c = ps->columns; c; c = c->next) {
            int w = c->width;
            if (w > 0) { // ignore collapsed columns (hack for search window)
                if (ev_x >= x + w - 2 && ev_x <= x + w) {
                    gdk_window_set_cursor (widget->window, cursor_sz);
                    break;
                }
                else {
                    gdk_window_set_cursor (widget->window, NULL);
                }
            }
            else {
                gdk_window_set_cursor (widget->window, NULL);
            }
            x += w;
        }
    }
    return FALSE;
}

static int
ddb_listview_header_get_column_idx_for_coord (DdbListview *pl, int click_x) {
    int x = -pl->hscrollpos;
    DdbListviewColumn *c;
    int idx = 0;
    for (c = pl->columns; c; c = c->next, idx++) {
        int w = c->width;
        if (click_x >= x && click_x < x + w) {
            return idx;
        }
        x += w;
    }
    return -1;
}

gboolean
ddb_listview_header_button_press_event           (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data)
{
    DdbListview *ps = DDB_LISTVIEW (gtk_object_get_data (GTK_OBJECT (widget), "owner"));
//    ps->active_column = ddb_listview_header_get_column_for_coord (ps, event->x);
    if (event->button == 1) {
        // start sizing/dragging
        ps->header_dragging = -1;
        ps->header_sizing = -1;
        ps->header_dragpt[0] = event->x;
        ps->header_dragpt[1] = event->y;
        int x = -ps->hscrollpos;
        int i = 0;
        DdbListviewColumn *c;
        for (c = ps->columns; c; c = c->next, i++) {
            int w = c->width;
            if (event->x >= x + w - 2 && event->x <= x + w) {
                ps->header_sizing = i;
                ps->header_dragging = -1;
                break;
            }
            else if (event->x > x + 2 && event->x < x + w - 2) {
                // prepare to drag or sort
                ps->header_dragpt[0] = event->x - x;
                ps->header_prepare = 1;
                ps->header_dragging = i;
                ps->header_sizing = -1;
                ps->prev_header_x = event->x;
                break;
            }
            x += w;
        }
    }
    else if (event->button == 3) {
        int idx = ddb_listview_header_get_column_idx_for_coord (ps, event->x);
        ps->binding->header_context_menu (ps, idx);
    }
    ps->prev_header_x = -1;
    ps->last_header_motion_ev = -1;
    return FALSE;
}

gboolean
ddb_listview_header_button_release_event         (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data)
{
    DdbListview *ps = DDB_LISTVIEW (gtk_object_get_data (GTK_OBJECT (widget), "owner"));
    if (event->button == 1) {
        if (ps->header_prepare) {
            ps->header_sizing = -1;
            ps->header_dragging = -1;
            ps->header_prepare = 0;
            // sort
            DdbListviewColumn *c;
            int i = 0;
            int x = -ps->hscrollpos;
            int sorted = 0;
            for (c = ps->columns; c; c = c->next, i++) {
                int w = c->width;
                if (event->x > x + 2 && event->x < x + w - 2) {
                    int sort_order = c->sort_order;
                    if (!sort_order) {
                        c->sort_order = 1;
                    }
                    else if (sort_order == 1) {
                        c->sort_order = 2;
                    }
                    else if (sort_order == 2) {
                        c->sort_order = 1;
                    }
                    ps->binding->col_sort (i, c->sort_order, c->user_data);
                    sorted = 1;
                }
                else {
                    c->sort_order = 0;
                }
                x += w;
            }
            ddb_listview_refresh (ps, DDB_REFRESH_LIST | DDB_REFRESH_COLUMNS | DDB_EXPOSE_LIST | DDB_EXPOSE_COLUMNS);
        }
        else {
            ps->header_sizing = -1;
            int x = 0;
            DdbListviewColumn *c;
            for (c = ps->columns; c; c = c->next) {
                int w = c->width;
                if (event->x >= x + w - 2 && event->x <= x + w) {
                    gdk_window_set_cursor (widget->window, cursor_sz);
                    break;
                }
                else {
                    gdk_window_set_cursor (widget->window, NULL);
                }
                x += w;
            }
            if (ps->header_dragging >= 0) {
                ps->header_dragging = -1;
                ddb_listview_refresh (ps, DDB_REFRESH_LIST | DDB_REFRESH_COLUMNS | DDB_EXPOSE_LIST | DDB_EXPOSE_COLUMNS | DDB_REFRESH_HSCROLL);
            }
        }
    }
    return FALSE;
}

struct set_cursor_t {
    int cursor;
    int prev;
    DdbListview *pl;
};

static gboolean
ddb_listview_set_cursor_cb (gpointer data) {
    struct set_cursor_t *sc = (struct set_cursor_t *)data;
    sc->pl->binding->set_cursor (sc->cursor);
    ddb_listview_select_single (sc->pl, sc->cursor);
    DdbListviewIter it;
//    int minvis = sc->pl->scrollpos;
//    int maxvis = sc->pl->scrollpos + sc->pl->nvisiblerows-1;
    DdbListview *ps = sc->pl;
//    if (sc->prev >= minvis && sc->prev <= maxvis)
    {
        it = sc->pl->binding->get_for_idx (sc->prev);
        ddb_listview_draw_row (sc->pl, sc->prev, it);
        UNREF (it);
    }
//    if (sc->cursor >= minvis && sc->cursor <= maxvis)
    {
        it = sc->pl->binding->get_for_idx (sc->cursor);
        ddb_listview_draw_row (sc->pl, sc->cursor, it);
        UNREF (it);
    }

    int cursor_scroll = ddb_listview_get_row_pos (sc->pl, sc->cursor);
    int newscroll = sc->pl->scrollpos;
    if (cursor_scroll < sc->pl->scrollpos) {
        newscroll = cursor_scroll;
    }
    else if (cursor_scroll >= sc->pl->scrollpos + sc->pl->list->allocation.height) {
        newscroll = cursor_scroll - sc->pl->list->allocation.height + 1;
        if (newscroll < 0) {
            newscroll = 0;
        }
    }
    if (sc->pl->scrollpos != newscroll) {
        GtkWidget *range = sc->pl->scrollbar;
        gtk_range_set_value (GTK_RANGE (range), newscroll);
    }

    free (data);
    return FALSE;
}

void
ddb_listview_set_cursor (DdbListview *pl, int cursor) {
    int prev = pl->binding->cursor ();
    struct set_cursor_t *data = malloc (sizeof (struct set_cursor_t));
    data->prev = prev;
    data->cursor = cursor;
    data->pl = pl;
    g_idle_add (ddb_listview_set_cursor_cb, data);
}

gboolean
ddb_listview_list_button_press_event         (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data)
{
    DdbListview *ps = DDB_LISTVIEW (gtk_object_get_data (GTK_OBJECT (widget), "owner"));
    if (event->button == 1) {
        ddb_listview_list_mouse1_pressed (ps, event->state, event->x, event->y, event->time);
    }
    else if (event->button == 3) {
        // get item under cursor
        DdbListviewGroup *grp;
        int grp_index;
        int sel;
        DdbListviewIter it = NULL;
        if (ddb_listview_list_pickpoint_y (ps, event->y + ps->scrollpos, &grp, &grp_index, &sel) != -1) {
            if (sel == -1) {
                sel = ps->binding->get_idx (grp->head);
            }
            it = ps->binding->get_for_idx (sel);
        }
        int y = sel;
        if (!it) {
            // clicked empty space -- deselect everything and show insensitive menu
            ps->binding->set_cursor (-1);
            it = ps->binding->head ();
            int idx = 0;
            while (it) {
                ps->binding->select (it, 0);
                ddb_listview_draw_row (ps, idx, it);
                ps->binding->selection_changed (it, idx);
                it = PL_NEXT (it);
                idx++;
            }
            // no menu
        }
        else {
            if (!ps->binding->is_selected (it)) {
                // item is unselected -- reset selection and select this
                DdbListviewIter it2 = ps->binding->head ();
                int idx = 0;
                ps->binding->set_cursor (y);
                while (it2) {
                    if (ps->binding->is_selected (it2) && it2 != it) {
                        ps->binding->select (it2, 0);
                        ddb_listview_draw_row (ps, idx, it2);
                        ps->binding->selection_changed (it2, idx);
                    }
                    else if (it2 == it) {
                        ps->binding->select (it2, 1);
                        ddb_listview_draw_row (ps, idx, it2);
                        ps->binding->selection_changed (it2, idx);
                    }
                    it2 = PL_NEXT (it2);
                    idx++;
                }
            }
            else {
                // something is selected; move cursor but keep selection
                ps->binding->set_cursor (y);
                ddb_listview_draw_row (ps, y, it);
            }
            ps->binding->list_context_menu (ps, it, y);
        }
    }
    return FALSE;
}

gboolean
ddb_listview_list_button_release_event       (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data)
{
    DdbListview *ps = DDB_LISTVIEW (gtk_object_get_data (GTK_OBJECT (widget), "owner"));
    if (event->button == 1) {
        ddb_listview_list_mouse1_released (ps, event->state, event->x, event->y, event->time);
    }
    return FALSE;
}

gboolean
ddb_listview_motion_notify_event        (GtkWidget       *widget,
                                        GdkEventMotion  *event,
                                        gpointer         user_data)
{
    DdbListview *ps = DDB_LISTVIEW (gtk_object_get_data (GTK_OBJECT (widget), "owner"));
    ddb_listview_list_mousemove (ps, event);
    return FALSE;
}

void
ddb_listview_set_binding (DdbListview *listview, DdbListviewBinding *binding) {
    listview->binding = binding;
}

DdbListviewIter
ddb_listview_get_iter_from_coord (DdbListview *listview, int x, int y) {
    DdbListviewGroup *grp;
    int grp_index;
    int sel;
    DdbListviewIter it = NULL;
    if (ddb_listview_list_pickpoint_y (listview, y + listview->scrollpos, &grp, &grp_index, &sel) != -1) {
        if (sel == -1) {
            sel = listview->binding->get_idx (grp->head);
        }
        it = listview->binding->get_for_idx (sel);
    }
    return it;
}

void
ddb_listview_scroll_to (DdbListview *listview, int pos) {
    pos = ddb_listview_get_row_pos (listview, pos);
    if (pos < listview->scrollpos || pos >= listview->scrollpos + listview->list->allocation.height) {
        gtk_range_set_value (GTK_RANGE (listview->scrollbar), pos - listview->list->allocation.height/2);
    }
}
int
ddb_listview_is_scrolling (DdbListview *listview) {
    return listview->dragwait;
}

// columns
void
ddb_listview_column_insert (DdbListview *listview, int before, const char *title, int width, int align_right, void *user_data);
void
ddb_listview_column_remove (DdbListview *listview, int idx);

/////// column management code

DdbListviewColumn * 
ddb_listview_column_alloc (const char *title, int width, int align_right, void *user_data) {
    DdbListviewColumn * c = malloc (sizeof (DdbListviewColumn));
    memset (c, 0, sizeof (DdbListviewColumn));
    c->title = strdup (title);
    c->width = width;
    c->align_right = align_right;
    c->user_data = user_data;
    return c;
}

int
ddb_listview_column_get_count (DdbListview *listview) {
    int cnt = 0;
    DdbListviewColumn *c = listview->columns;
    while (c) {
        cnt++;
        c = c->next;
    }
    return cnt;
}

void
ddb_listview_column_append (DdbListview *listview, const char *title, int width, int align_right, void *user_data) {
    DdbListviewColumn* c = ddb_listview_column_alloc (title, width, align_right, user_data);
    int idx = 0;
    DdbListviewColumn * columns = listview->columns;
    if (columns) {
        idx++;
        DdbListviewColumn * tail = listview->columns;
        while (tail->next) {
            tail = tail->next;
            idx++;
        }
        tail->next = c;
    }
    else {
        listview->columns = c;
    }
    listview->binding->columns_changed (listview);
}

void
ddb_listview_column_insert (DdbListview *listview, int before, const char *title, int width, int align_right, void *user_data) {
    DdbListviewColumn *c = ddb_listview_column_alloc (title, width, align_right, user_data);
    if (listview->columns) {
        DdbListviewColumn * prev = NULL;
        DdbListviewColumn * next = listview->columns;
        int idx = 0;
        while (next) {
            if (idx == before) {
                break;
            }
            prev = next;
            next = next->next;
            idx++;
        }
        c->next = next;
        if (prev) {
            prev->next = c;
        }
        else {
            listview->columns = c;
        }
    }
    else {
        listview->columns = c;
    }
    listview->binding->columns_changed (listview);
}

void
ddb_listview_column_free (DdbListview *listview, DdbListviewColumn * c) {
    if (c->title) {
        free (c->title);
    }
    listview->binding->col_free_user_data (c->user_data);
    free (c);
}

void
ddb_listview_column_remove (DdbListview *listview, int idx) {
    DdbListviewColumn *c;
    if (idx == 0) {
        c = listview->columns;
        assert (c);
        listview->columns = c->next;
        ddb_listview_column_free (listview, c);
        return;
    }
    c = listview->columns;
    int i = 0;
    while (c) {
        if (i+1 == idx) {
            assert (c->next);
            DdbListviewColumn *next = c->next->next;
            ddb_listview_column_free (listview, c->next);
            c->next = next;
            return;
        }
        c = c->next;
        idx++;
    }

    if (!c) {
        trace ("ddblv: attempted to remove column that is not in list\n");
    }
    listview->binding->columns_changed (listview);
}

void
ddb_listview_column_move (DdbListview *listview, DdbListviewColumn *which, int inspos) {
    // remove c from list
    DdbListviewColumn *c = (DdbListviewColumn *)which;
    if (c == listview->columns) {
        listview->columns = c->next;
    }
    else {
        DdbListviewColumn *cc;
        for (cc = listview->columns; cc; cc = cc->next) {
            if (cc->next == c) {
                cc->next = c->next;
                break;
            }
        }
    }
    c->next = NULL;
    // reinsert c at position inspos update header_dragging to new idx
    if (inspos == 0) {
        c->next = listview->columns;
        listview->columns = c;
    }
    else {
        int idx = 0;
        DdbListviewColumn *prev = NULL;
        DdbListviewColumn *cc = NULL;
        for (cc = listview->columns; cc; cc = cc->next, idx++, prev = cc) {
            if (idx+1 == inspos) {
                DdbListviewColumn *next = cc->next;
                cc->next = c;
                c->next = next;
                break;
            }
        }
    }
    listview->binding->columns_changed (listview);
}

int
ddb_listview_column_get_info (DdbListview *listview, int col, const char **title, int *width, int *align_right, void **user_data) {
    DdbListviewColumn *c;
    int idx = 0;
    for (c = listview->columns; c; c = c->next, idx++) {
        if (idx == col) {
            *title = c->title;
            *width = c->width;
            *align_right = c->align_right;
            *user_data = c->user_data;
            return 0;
        }
    }
    return -1;
}

int
ddb_listview_column_set_info (DdbListview *listview, int col, const char *title, int width, int align_right, void *user_data) {
    DdbListviewColumn *c;
    int idx = 0;
    for (c = listview->columns; c; c = c->next, idx++) {
        if (idx == col) {
            free (c->title);
            c->title = strdup (title);
            c->width = width;
            c->align_right = align_right;
            c->user_data = user_data;
            return 0;
        }
    }
    return -1;
}
/////// end of column management code

/////// grouping /////
void
ddb_listview_free_groups (DdbListview *listview) {
    DdbListviewGroup *next;
    while (listview->groups) {
        next = listview->groups->next;
        if (listview->groups->head) {
            listview->binding->unref (listview->groups->head);
        }
        free (listview->groups);
        listview->groups = next;
    }
}

void
ddb_listview_build_groups (DdbListview *listview) {
    ddb_listview_free_groups (listview);
    listview->fullheight = 0;

    DdbListviewGroup *grp = NULL;
    char str[1024];
    char curr[1024];

    GROUP_TITLE_HEIGHT = DEFAULT_GROUP_TITLE_HEIGHT;
    DdbListviewIter it = listview->binding->head ();
    while (it) {
        int res = listview->binding->get_group (it, curr, sizeof (curr));
        if (res == -1) {
            grp = malloc (sizeof (DdbListviewGroup));
            listview->groups = grp;
            memset (grp, 0, sizeof (DdbListviewGroup));
            grp->head = it;
            listview->binding->ref (it);
            grp->num_items = listview->binding->count ();
            listview->fullheight = grp->num_items * listview->rowheight;
            GROUP_TITLE_HEIGHT = 0;
            listview->fullheight += GROUP_TITLE_HEIGHT;
            return;
        }
        if (!grp || strcmp (str, curr)) {
            strcpy (str, curr);
            DdbListviewGroup *newgroup = malloc (sizeof (DdbListviewGroup));
            if (grp) {
                grp->next = newgroup;
            }
            else {
                listview->groups = newgroup;
            }
            grp = newgroup;
            memset (grp, 0, sizeof (DdbListviewGroup));
            grp->head = it;
            listview->binding->ref (it);
            grp->num_items = 0;
            listview->fullheight += GROUP_TITLE_HEIGHT;
        }
        grp->num_items++;
        listview->fullheight += listview->rowheight;
        DdbListviewIter next = listview->binding->next (it);
        listview->binding->unref (it);
        it = next;
    }
}