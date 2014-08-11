#include "bcbtree_range_count.h"
#include "disk.h"

#include <gtk/gtk.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <set>

using namespace std;

// statusbar items
#define NO_OF_STATUSBARITEM	16
#define MOUSE_LABEL	0
#define MOUSE		1
#define COUNT_LABEL	2
#define COUNT		3
#define SEPARATOR1	4
#define X1_LABEL	5
#define X1			6
#define Y1_LABEL	7
#define Y1			8
#define X2_LABEL	9
#define X2			10
#define Y2_LABEL	11
#define Y2			12
#define SCALE_LABEL	13
#define SCALE		14
#define QUERY_BTN	15
#define ENTRY_SIZE	70

// control state
#define SELECT_NONE			0
#define SELECTING			1
#define SELECTED			2
#define DRAG_MAP			3
#define SELECTED_DRAG_MAP	4
#define ENTERING_TEXT		5

// server thread state
#define NO_REQUEST	0
#define BUILD_REQUEST	1
#define QUERY_REQUEST	2
#define PROCESSING		3
#define	PROCESSING_1	13
#define	PROCESSING_2	23
#define	PROCESSING_3	33
#define	PROCESSING_4	43

// refresh rate (Hz)
#define FRAME_RATE	100

// grid interval
#define GRID_INTERVAL	100

// epsilon
#define EPSILON	1

// block size
#define BLOCK_SIZE	4096
#define BLOCK_SIZE_8	(BLOCK_SIZE / 8)

// widgets
GtkWidget* window;								//main window
GtkWidget* statusBarItem[NO_OF_STATUSBARITEM];	//statusbar

// timer counter
guint timer = 0;

// files
gchar* source_file_name = NULL;
gchar* structure_file_name = NULL;

// control
gint control_state = 0;	//see #define
gint pointer_x = 0;
gint pointer_y = 0;
gint drag_map_starting_x = 0;
gint drag_map_starting_y = 0;
gint select_starting_x = 0;
gint select_starting_y = 0;
gint select_current_x = 0;
gint select_current_y = 0;

// server
gint server_state = 0;	//see #define
GCond* server_cond = NULL;
GMutex* server_mutex = NULL;
gint query_x1 = 0;
gint query_y1 = 0;
gint query_x2 = 0;
gint query_y2 = 0;
gint query_result = 0;

// window view
gdouble scale = 1.0;
gint offset_x = 0;
gint offset_y = 0;
gint drawing_area_width = 0;
gint drawing_area_height = 0;
gint grid_interval = GRID_INTERVAL;
cairo_surface_t* bg_surface = NULL;

// togglers
gboolean show_points = TRUE;
gboolean update_background = TRUE;
gboolean update_screen = FALSE;
gboolean update_selecting = FALSE;
gboolean update_points = FALSE;
gboolean structure_changed = FALSE;

// points
struct pt_comp{
	bool operator()(const GdkPoint a, const GdkPoint b){
		if(a.x > b.x)	return true;
		else if(a.x == b.x)	return a.y > b.y;
		else return false;
	}
};
set< GdkPoint, pt_comp > points_set;

GCond* points_updater_cond = NULL;
GMutex* points_updater_mutex = NULL;

void query(gint x1, gint y1, gint x2, gint y2){
	
	if(server_state == BUILD_REQUEST || structure_file_name == NULL){
		return;
	}
	
	// send query request to bcbtree_range_count server and wake it up
	g_mutex_lock(server_mutex);
	if(x1 < x2){
		query_x1 = x1 - EPSILON;
		query_x2 = x2;
	}else{
		query_x1 = x2 - EPSILON;
		query_x2 = x1;
	}
	if(y1 < y2){
		query_y1 = y1 - EPSILON;
		query_y2 = y2;
	}else{
		query_y1 = y2 - EPSILON;
		query_y2 = y1;
	}
	if(server_state == NO_REQUEST){
		server_state = QUERY_REQUEST;
		g_cond_signal(server_cond);
	}else{
		server_state = QUERY_REQUEST;
	}
	g_mutex_unlock(server_mutex);
	
	return;
}

/* timer function to control threads and display */
static gboolean time_handler(GtkWidget* widget){
	static gint timeout = 0;
	
	timer++;
	
	// send update request to points_updater and wake it up
	if(update_points == TRUE){
		g_mutex_lock(points_updater_mutex);
		g_cond_signal(points_updater_cond);
		g_mutex_unlock(points_updater_mutex);
	}
	
	// send build request to bcbtree_range_count server and wake it up
	if(server_state == BUILD_REQUEST){
		// g_mutex_lock(server_mutex);
		g_cond_signal(server_cond);
		// g_mutex_unlock(server_mutex);
	}
	
	// send query request if exceed timeout
	if(control_state == SELECTING && update_selecting == TRUE){
		timeout++;
		if(timeout >= 10){
			update_selecting = FALSE;
			query(select_starting_x, select_starting_y, select_current_x, select_current_y);
			timeout = 0;
		}
	}
	
	// update screen
	if(update_background == TRUE || update_screen == TRUE || server_state == BUILD_REQUEST){
		update_screen = FALSE;
		gtk_widget_queue_draw(widget);
	}
	
	return TRUE;
}

void itemPressed(GtkMenuItem *menuItem, gpointer data) {
    
	// open file
	if(!strcmp((gchar*)data, "open_file")){
		GtkWidget* fileChooserDialog;
		gchar* file_name = NULL;
		gint file_name_length = 0;
		gchar* tmp_str = NULL;
		
		// show file chooser dialog
		fileChooserDialog = gtk_file_chooser_dialog_new("Open File", NULL, 
														GTK_FILE_CHOOSER_ACTION_OPEN, 
														GTK_STOCK_CANCEL, 
														GTK_RESPONSE_CANCEL, 
														GTK_STOCK_OPEN, 
														GTK_RESPONSE_ACCEPT, NULL);
		
		gint fileChooserDialog_result = gtk_dialog_run(GTK_DIALOG(fileChooserDialog));
		
		if(fileChooserDialog_result == GTK_RESPONSE_ACCEPT){
			file_name = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(fileChooserDialog));
			gtk_widget_destroy(fileChooserDialog);
		}else{
			gtk_widget_destroy(fileChooserDialog);
			return;
		}
		
		// check if the file is a .rcstruct file
		file_name_length = (gint)strlen(file_name);
		
		if(file_name_length >= 9 && !strcmp(file_name + (file_name_length - 9), ".rcstruct")){
			// already a .rcstruct structure file no need to build structure
			tmp_str = (gchar*) malloc(sizeof(gchar) * file_name_length);
			sprintf(tmp_str, "%s", file_name);
			if(structure_file_name != NULL){
				g_free(structure_file_name);
			}
			structure_file_name = tmp_str;
			update_background = TRUE;
			update_points = TRUE;
			structure_changed = TRUE;
		}else{
			// raw data file, need to build structure
			tmp_str = (gchar*) malloc(sizeof(gchar) * file_name_length);
			sprintf(tmp_str, "%s", file_name);
			if(source_file_name != NULL){
				g_free(source_file_name);
			}
			source_file_name = tmp_str;
			tmp_str = (gchar*) malloc(sizeof(gchar) * (file_name_length + 10));
			sprintf(tmp_str, "%s.rcstruct", file_name);
			if(structure_file_name != NULL){
				g_free(structure_file_name);
			}
			structure_file_name = tmp_str;
			server_state = BUILD_REQUEST;
		}
	}else if(!strcmp((gchar*)data, "about")){
		GtkWidget* aboutDialog;
		
		aboutDialog = gtk_about_dialog_new();
		gtk_about_dialog_set_name(GTK_ABOUT_DIALOG(aboutDialog), "Range Count in External Memory");
		// gtk_about_dialog_set_comments(GTK_ABOUT_DIALOG(aboutDialog), "The Chinese University of Hong Kong\nDepartment of Computer Science and Engineering\nFinal Year Project");
		gtk_about_dialog_set_comments(GTK_ABOUT_DIALOG(aboutDialog), "CUHK CSE FYP");
		gtk_about_dialog_set_copyright(GTK_ABOUT_DIALOG(aboutDialog), "by Leo Yu Ho, Lo under Yufei Tao's supervision");
		
		gtk_dialog_run(GTK_DIALOG(aboutDialog));
		gtk_widget_destroy(aboutDialog);
	}
}

void itemToggled(GtkCheckMenuItem* checkMenuItem, gpointer data){
	
	// toggle show_points
	if(!strcmp((gchar*)data, "show_points")){
		if(gtk_check_menu_item_get_active(checkMenuItem)){
			show_points = TRUE;
		}else{
			show_points = FALSE;
		}
		update_background = TRUE;
	}
}

gboolean drawing_area_event_handler(GtkWidget* widget, GdkEvent* event, gpointer data){
	static gint width;
	static gint height;
	static gdouble old_scale;
	static GdkModifierType state;
	static GdkEventType type;
	static GdkEventScroll* eventScr;
	static GdkEventButton* eventBtn;
	
	if(server_state == BUILD_REQUEST){
		return FALSE;
	}
	
	gdk_drawable_get_size(widget->window, &width, &height);
	type = event->type;
	
	gdk_window_get_pointer(((GdkEventMotion*)event)->window, &pointer_x, &pointer_y, &state);
	if(pointer_x > width)	pointer_x = width;
	if(pointer_x < 0)	pointer_x = 0;
	if(pointer_y > height)	pointer_y = height;
	if(pointer_y < 0)	pointer_y = 0;
	// co-ordinate conversion
	pointer_x = (gint)((gdouble)pointer_x * scale) + offset_x;
	pointer_y = (gint)((gdouble)(height - pointer_y) * scale) + offset_y;
	
	// if mouse move
	if(type == GDK_MOTION_NOTIFY || type == GDK_LEAVE_NOTIFY){
		if(control_state == DRAG_MAP || control_state == SELECTED_DRAG_MAP){
			offset_x -= (pointer_x - drag_map_starting_x);
			offset_y -= (pointer_y - drag_map_starting_y);
			update_background = TRUE;
			update_points = TRUE;
		}else if(control_state == SELECTING){
			select_current_x = pointer_x;
			select_current_y = pointer_y;
			update_selecting = TRUE;
		}
		
	// if scroll
	}else if(type == GDK_SCROLL){
		eventScr = (GdkEventScroll*) event;
		old_scale = scale;
		
		if(eventScr->direction == GDK_SCROLL_UP){
			if(scale == 5){
				grid_interval /= 5;
			}
			scale -= 1;
			if(scale < 1){
				scale = 1;
			}
		}else if(eventScr->direction == GDK_SCROLL_DOWN){
			scale += 1;
			if(scale == 5){
				grid_interval *= 5;
			}
			if(scale > 20){
				scale = 20;
			}
		}
		
		gdk_window_get_pointer(((GdkEventMotion*)event)->window, &pointer_x, &pointer_y, &state);
		offset_x = offset_x - (gint)((gdouble)pointer_x * (scale - old_scale));
		offset_y = offset_y - (gint)((gdouble)(height - pointer_y) * (scale - old_scale));
		pointer_x = (gint)((gdouble)pointer_x * scale) + offset_x;
		pointer_y = (gint)((gdouble)(height - pointer_y) * scale) + offset_y;
		update_background = TRUE;
		update_points = TRUE;
		
	// if left button pressed
	}else if(type == GDK_BUTTON_PRESS){
		eventBtn = (GdkEventButton*) event;
		
		if(eventBtn->button == 1){
			if(state & GDK_CONTROL_MASK){
				drag_map_starting_x = pointer_x;
				drag_map_starting_y = pointer_y;
				if(control_state == SELECTED){
					control_state = SELECTED_DRAG_MAP;
				}else{
					control_state = DRAG_MAP;
				}
			}else{
				select_starting_x = pointer_x;
				select_starting_y = pointer_y;
				select_current_x = pointer_x;
				select_current_y = pointer_y;
				control_state = SELECTING;
			}
		}
		
	// if left button released
	}else if(type = GDK_BUTTON_RELEASE){
		eventBtn = (GdkEventButton*) event;
		
		if(eventBtn->button == 1){
			if(control_state == DRAG_MAP){
				control_state = SELECT_NONE;
			}else if(control_state == SELECTED_DRAG_MAP){
				control_state = SELECTED;
			}else{
				select_current_x = pointer_x;
				select_current_y = pointer_y;
				if(select_current_x == select_starting_x && select_current_y == select_starting_y){
					control_state = SELECT_NONE;
				}else{
					control_state = SELECTED;
					query(select_starting_x, select_starting_y, select_current_x, select_current_y);
				}
			}
		}
	}
	
	// update screen
	update_screen = TRUE;
	
	return FALSE;
}

gboolean drawing_area_expose_event_callback(GtkWidget *widget, GdkEventExpose *event, gpointer data){
	static GdkRectangle selected_rect;
	static cairo_t* cr;
	static cairo_t* loading_cr;
	static cairo_surface_t* loading_surface;
	static cairo_t* bg_cr;
	static GdkPoint pt;
	static set< GdkPoint >::iterator it;
	static gint offset_x2;
	static gint offset_y2;
	
	gdk_drawable_get_size(widget->window, &drawing_area_width, &drawing_area_height);
	
	cr = gdk_cairo_create(widget->window);
	
	// re-draw background if needed
	if(update_background == TRUE){
		update_background = FALSE;
		
		if(bg_surface != NULL){
			cairo_surface_destroy(bg_surface);
		}
		
		bg_surface = cairo_surface_create_similar(cairo_get_target(cr), CAIRO_CONTENT_COLOR_ALPHA, drawing_area_width, drawing_area_height);
		bg_cr = cairo_create(bg_surface);
		
		// draw grids
		gint i;
		gint start;
		gdouble draw;
		start = (offset_x / grid_interval) * grid_interval;
		for(i = start;((i - offset_x) / scale) < drawing_area_width;i += grid_interval){
			if(i % (grid_interval * 5) == 0){
				
				cairo_set_source_rgb(bg_cr, 0, 0, 0);
				cairo_set_line_width(bg_cr, 0.5);
			}else{
				
				cairo_set_source_rgb(bg_cr, 0.5, 0.5, 0.5);
				cairo_set_line_width(bg_cr, 0.25);
			}
			draw = (i - offset_x) / scale;
			cairo_move_to(bg_cr, draw, 0);
			cairo_line_to(bg_cr, draw, drawing_area_height);
			cairo_stroke(bg_cr);
		}
		
		start = (offset_y / grid_interval) * grid_interval;
		for(i = start;((i - offset_y) / scale) < drawing_area_height;i += grid_interval){
			if(i % (grid_interval * 5) == 0){
				
				cairo_set_source_rgb(bg_cr, 0, 0, 0);
				cairo_set_line_width(bg_cr, 0.5);
			}else{
				
				cairo_set_source_rgb(bg_cr, 0.5, 0.5, 0.5);
				cairo_set_line_width(bg_cr, 0.25);
			}
			draw = drawing_area_height - ((i - offset_y) / scale);
			cairo_move_to(bg_cr, 0, draw);
			cairo_line_to(bg_cr, drawing_area_width, draw);
			cairo_stroke(bg_cr);
		}
		
		// draw points
		if(structure_file_name != NULL && show_points == TRUE){
			cairo_set_source_rgb(bg_cr, 0, 0, 0);
			cairo_set_line_width(bg_cr, 1);
			offset_x2 = offset_x + drawing_area_width * scale;
			offset_y2 = offset_y + drawing_area_height * scale;
			for(it = points_set.begin();it != points_set.end();it++){
				pt = *it;
				if(pt.x >= offset_x && pt.x <= offset_x2 && pt.y >= offset_y && pt.y <= offset_y2){
					pt.x = (pt.x - offset_x) / scale;
					pt.y = drawing_area_height - (pt.y - offset_y) / scale;
					cairo_move_to(bg_cr, pt.x - 5, pt.y - 10);
					cairo_line_to(bg_cr, pt.x, pt.y);
					cairo_line_to(bg_cr, pt.x + 5, pt.y - 10);
					cairo_close_path(bg_cr);
					cairo_stroke(bg_cr);
				}else{
					points_set.erase(it);
				}
			}
		}
		
		cairo_destroy(bg_cr);
	}
	cairo_set_source_surface(cr, bg_surface, 0, 0);
	cairo_paint(cr);
	
	// draw selecting rectangle if needed
	if((control_state != SELECT_NONE) && (select_starting_x != select_current_x || select_starting_y != select_current_y)){
		if(select_starting_x > select_current_x){
			selected_rect.x = select_current_x;
			selected_rect.width = select_starting_x - select_current_x;
		}else{
			selected_rect.x = select_starting_x;
			selected_rect.width = select_current_x - select_starting_x;
		}
		if(select_starting_y < select_current_y){
			selected_rect.y = select_current_y;
			selected_rect.height = select_current_y - select_starting_y;
		}else{
			selected_rect.y = select_starting_y;
			selected_rect.height = select_starting_y - select_current_y;
		}
		
		// co-ordinate conversion
		selected_rect.x = (gint)(((gdouble)(selected_rect.x - offset_x)) / scale);
		selected_rect.width = (gint)(((gdouble)selected_rect.width) / scale);
		selected_rect.y = drawing_area_height - (gint)(((gdouble)(selected_rect.y - offset_y)) / scale);
		selected_rect.height = (gint)(((gdouble)selected_rect.height) / scale);
		
		// out of drawing area
		if(selected_rect.x < 0){
			selected_rect.width = selected_rect.width + (selected_rect.x + 1);
			selected_rect.x = -1;
		}
		if(selected_rect.width > drawing_area_width){
			selected_rect.width = drawing_area_width + 2;
		}
		if(selected_rect.y < 0){
			selected_rect.height = selected_rect.height + (selected_rect.y + 1);
			selected_rect.y = -1;
		}
		if(selected_rect.height > drawing_area_height){
			selected_rect.height = drawing_area_height + 2;
		}
		
		cairo_set_source_rgba(cr, 0.0, 0.0, 1.0, 1);
		cairo_set_line_width(cr, 1);
		
		cairo_rectangle(cr, selected_rect.x, selected_rect.y, selected_rect.width, selected_rect.height);
		cairo_stroke_preserve(cr);
		
		cairo_set_source_rgba(cr, 0.0, 0.0, 1.0, 0.4);
		cairo_fill(cr);
	}
	
	// draw loading message if needed
	if(server_state == BUILD_REQUEST){
		cairo_text_extents_t extents;
		
		cairo_set_source_rgba(cr, 0.0, 0.0, 0.5, 0.6);
		cairo_select_font_face(cr, "Courier", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
		cairo_set_font_size(cr, 20);
		cairo_text_extents(cr, "Loading...", &extents);
		cairo_move_to(cr, (drawing_area_width - extents.width) / 2, (drawing_area_height - extents.height) / 2 + 60);
		cairo_text_path(cr, "Loading...");
		
		cairo_stroke(cr);
		
		cairo_save(cr);
		cairo_translate(cr, drawing_area_width / 2, drawing_area_height /2);
		
		static gdouble const transparent[8][8] = {
			{ 0.0, 0.15, 0.30, 0.5, 0.65, 0.80, 0.9, 1.0 },
			{ 1.0, 0.0,  0.15, 0.30, 0.5, 0.65, 0.8, 0.9 },
			{ 0.9, 1.0,  0.0,  0.15, 0.3, 0.5, 0.65, 0.8 },
			{ 0.8, 0.9,  1.0,  0.0,  0.15, 0.3, 0.5, 0.65},
			{ 0.65, 0.8, 0.9,  1.0,  0.0,  0.15, 0.3, 0.5 },
			{ 0.5, 0.65, 0.8, 0.9, 1.0,  0.0,  0.15, 0.3 },
			{ 0.3, 0.5, 0.65, 0.8, 0.9, 1.0,  0.0,  0.15 },
			{ 0.15, 0.3, 0.5, 0.65, 0.8, 0.9, 1.0,  0.0, }
		};
		
		gint i = 0;
		for (i = 0; i < 8; i++) {
			cairo_set_line_width(cr, 3);
			cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
			cairo_set_source_rgba(cr, 0, 0, 0.5, transparent[(timer / 10) % 8][i]);
			
			cairo_move_to(cr, 0.0, -10.0);
			cairo_line_to(cr, 0.0, -20.0);
			cairo_rotate(cr, M_PI/4);
			
			cairo_stroke(cr);
		}
		cairo_restore(cr);
	}
	
	cairo_destroy(cr);
	
	// update statusBar
	static gchar statusBar_Text[200];
	sprintf(statusBar_Text, "(%d, %d)", pointer_x, pointer_y);
	gtk_label_set_text(GTK_LABEL(((GtkWidget**)data)[MOUSE]), statusBar_Text);
	if(server_state == QUERY_REQUEST || (server_state % 10) == PROCESSING){
		sprintf(statusBar_Text, "Processing...");
		gtk_label_set_text(GTK_LABEL(((GtkWidget**)data)[COUNT]), statusBar_Text);
	}else if(control_state == SELECT_NONE){
		sprintf(statusBar_Text, "%d", 0);
		gtk_label_set_text(GTK_LABEL(((GtkWidget**)data)[COUNT]), statusBar_Text);
	}else{
		sprintf(statusBar_Text, "%d", query_result);
		gtk_label_set_text(GTK_LABEL(((GtkWidget**)data)[COUNT]), statusBar_Text);
	}
	if(control_state != SELECT_NONE){
		sprintf(statusBar_Text, "%d", select_starting_x);
		gtk_entry_set_text(GTK_ENTRY(((GtkWidget**)data)[X1]), statusBar_Text);
		sprintf(statusBar_Text, "%d", select_starting_y);
		gtk_entry_set_text(GTK_ENTRY(((GtkWidget**)data)[Y1]), statusBar_Text);
		sprintf(statusBar_Text, "%d", select_current_x);
		gtk_entry_set_text(GTK_ENTRY(((GtkWidget**)data)[X2]), statusBar_Text);
		sprintf(statusBar_Text, "%d", select_current_y);
		gtk_entry_set_text(GTK_ENTRY(((GtkWidget**)data)[Y2]), statusBar_Text);
	}
	sprintf(statusBar_Text, "%.1f", scale);
	gtk_entry_set_text(GTK_ENTRY(((GtkWidget**)data)[SCALE]), statusBar_Text);
	
	return FALSE;
}

gboolean query_btn_event_handler(GtkButton* btn, gpointer data){
	gint x1 = 0;
	gint y1 = 0;
	gint x2 = 0;
	gint y2 = 0;
	const gchar* tmp_str;
	
	tmp_str = gtk_entry_get_text(((GtkEntry**)data)[X1]);
	if(isdigit(tmp_str[0]) || tmp_str[0] == '-'){
		x1 = atoi(tmp_str);
	}
	tmp_str = gtk_entry_get_text(((GtkEntry**)data)[Y1]);
	if(isdigit(tmp_str[0]) || tmp_str[0] == '-'){
		y1 = atoi(tmp_str);
	}
	tmp_str = gtk_entry_get_text(((GtkEntry**)data)[X2]);
	if(isdigit(tmp_str[0]) || tmp_str[0] == '-'){
		x2 = atoi(tmp_str);
	}
	tmp_str = gtk_entry_get_text(((GtkEntry**)data)[Y2]);
	if(isdigit(tmp_str[0]) || tmp_str[0] == '-'){
		y2 = atoi(tmp_str);
	}
	
	query(x1, y1, x2, y2);
	
	select_starting_x = x1;
	select_starting_y = y1;
	select_current_x = x2;
	select_current_y = y2;
	
	control_state = SELECTED;
	
	return FALSE;
}

GtkWidget* createStatusBar(GtkWidget** statusBarItem){
	GtkWidget* statusBar;
	
	statusBarItem[MOUSE_LABEL] = gtk_label_new(" Mouse: ");
	statusBarItem[MOUSE] = gtk_label_new("(0, 0)");
	statusBarItem[COUNT_LABEL] = gtk_label_new(" Count: ");
	statusBarItem[COUNT] = gtk_label_new("0");
	statusBarItem[SEPARATOR1] = gtk_label_new("");
	gtk_widget_set_size_request(statusBarItem[SEPARATOR1], -1, -1);
	statusBarItem[X1_LABEL] = gtk_label_new(" X1: ");
	statusBarItem[X1] = gtk_entry_new();
	gtk_widget_set_size_request(statusBarItem[X1], ENTRY_SIZE, -1);
	statusBarItem[Y1_LABEL] = gtk_label_new(" Y1: ");
	statusBarItem[Y1] = gtk_entry_new();
	gtk_widget_set_size_request(statusBarItem[Y1], ENTRY_SIZE, -1);
	statusBarItem[X2_LABEL] = gtk_label_new(" X2: ");
	statusBarItem[X2] = gtk_entry_new();
	gtk_widget_set_size_request(statusBarItem[X2], ENTRY_SIZE, -1);
	statusBarItem[Y2_LABEL] = gtk_label_new(" Y2: ");
	statusBarItem[Y2] = gtk_entry_new();
	gtk_widget_set_size_request(statusBarItem[Y2], ENTRY_SIZE, -1);
	statusBarItem[SCALE_LABEL] = gtk_label_new(" Scale: ");
	statusBarItem[SCALE] = gtk_entry_new();
	gtk_widget_set_size_request(statusBarItem[SCALE], ENTRY_SIZE / 2.0, -1);
	statusBarItem[QUERY_BTN] = gtk_button_new_with_label("Query");
	gtk_widget_set_size_request(statusBarItem[QUERY_BTN], ENTRY_SIZE, -1);
	
	statusBar = gtk_hbox_new(FALSE, 0);
	for(gint i = 0;i < NO_OF_STATUSBARITEM;i++){
		
		if(i == SEPARATOR1){
			gtk_box_pack_start(GTK_BOX(statusBar), statusBarItem[i], TRUE, TRUE, 0);
		}else{
			gtk_box_pack_start(GTK_BOX(statusBar), statusBarItem[i], FALSE, FALSE, 0);
		}
	}
	g_signal_connect(GTK_OBJECT(statusBarItem[QUERY_BTN]), "clicked", G_CALLBACK(query_btn_event_handler), statusBarItem);
	
	return statusBar;
}

GtkWidget* createAll(){
	
	GtkWidget* vbox;
	GtkWidget* menubar;
	GtkWidget* fileMenu;
	GtkWidget* rootFileMenuItem;
	GtkWidget* openFileMenuItem;
	GtkWidget* exitFileMenuItem;
	GtkWidget* viewMenu;
	GtkWidget* rootViewMenuItem;
	GtkWidget* showPointsViewMenuItem;
	GtkWidget* aboutMenu;
	GtkWidget* statusBar;
	GtkWidget* drawing_area;
	
	/* menu bar */
	
	/* File */
	rootFileMenuItem = gtk_menu_item_new_with_mnemonic("_File");
	openFileMenuItem = gtk_menu_item_new_with_label("Open");
	exitFileMenuItem = gtk_menu_item_new_with_label("Eixt");
	g_signal_connect(GTK_OBJECT(openFileMenuItem), "activate", G_CALLBACK(itemPressed), (gpointer*)"open_file");
	g_signal_connect(GTK_OBJECT(exitFileMenuItem), "activate", G_CALLBACK(gtk_main_quit), NULL);
	
	fileMenu = gtk_menu_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(fileMenu), openFileMenuItem);
	gtk_menu_shell_append(GTK_MENU_SHELL(fileMenu), gtk_separator_menu_item_new());
	gtk_menu_shell_append(GTK_MENU_SHELL(fileMenu), exitFileMenuItem);
	
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(rootFileMenuItem), fileMenu);
	
	/* View */
	rootViewMenuItem = gtk_menu_item_new_with_mnemonic("_View");
	showPointsViewMenuItem = gtk_check_menu_item_new_with_label("Show Points");
	if(show_points == TRUE){
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(showPointsViewMenuItem), TRUE);
	}else{
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(showPointsViewMenuItem), FALSE);
	}
	g_signal_connect(GTK_OBJECT(showPointsViewMenuItem), "toggled", G_CALLBACK(itemToggled), (gpointer*)"show_points");
	
	viewMenu = gtk_menu_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(viewMenu), showPointsViewMenuItem);
	
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(rootViewMenuItem), viewMenu);
	
	/* About */
	aboutMenu = gtk_menu_item_new_with_mnemonic("_About");
	g_signal_connect(GTK_OBJECT(aboutMenu), "activate", G_CALLBACK(itemPressed), (gpointer*)"about");
	
	// add all menus to menu bar
    menubar = gtk_menu_bar_new();
    gtk_menu_bar_append(menubar, rootFileMenuItem);
    gtk_menu_bar_append(menubar, rootViewMenuItem);
    gtk_menu_bar_append(menubar, aboutMenu);
	
	/* labels used as status bar */
	statusBar = createStatusBar(statusBarItem);
	
	/* drawing area */
	drawing_area = gtk_drawing_area_new();
    gtk_widget_set_size_request(drawing_area, 880, 660);
	gtk_widget_add_events(drawing_area, GDK_POINTER_MOTION_MASK | 
	GDK_POINTER_MOTION_HINT_MASK | 
	GDK_LEAVE_NOTIFY_MASK | 
	GDK_BUTTON_PRESS_MASK | 
	GDK_BUTTON_RELEASE_MASK | 
	GDK_SCROLL_MASK);
    g_signal_connect(GTK_OBJECT(drawing_area), "expose_event", G_CALLBACK(drawing_area_expose_event_callback), statusBarItem);
	g_signal_connect(GTK_OBJECT(drawing_area), "motion_notify_event", G_CALLBACK(drawing_area_event_handler), statusBarItem);
	g_signal_connect(GTK_OBJECT(drawing_area), "leave_notify_event", G_CALLBACK(drawing_area_event_handler), statusBarItem);
	g_signal_connect(GTK_OBJECT(drawing_area), "button_press_event", G_CALLBACK(drawing_area_event_handler), statusBarItem);
	g_signal_connect(GTK_OBJECT(drawing_area), "button_release_event", G_CALLBACK(drawing_area_event_handler), statusBarItem);
	g_signal_connect(GTK_OBJECT(drawing_area), "scroll_event", G_CALLBACK(drawing_area_event_handler), statusBarItem);
	
	/* vbox */
    vbox = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), menubar, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), drawing_area, TRUE, TRUE, 0);
	gtk_box_pack_end(GTK_BOX(vbox), statusBar, FALSE, FALSE, 0);
	
	return vbox;
}

/* thread for reading points data from disk */
gpointer points_updater(gpointer data){
	gboolean idle = FALSE;
	disk* storage = NULL;
	int read_block_id = 0;
	int total_records = 0;
	int leaf_start_block_id = 0;
	int leaf_end_block_id = 0;
	int btree_root_block_id = 0;
	char metadata_block[BLOCK_SIZE] = {0};
	char read_block[BLOCK_SIZE] = {0};
	char* read_block_cur_pos = NULL;
	char* read_block_end_pos = NULL;
	int x[2] = {-1};
	int last_x[2] = {-1};
	int last_y[2] = {-1};
	int offset_x2 = 0;
	int offset_y2 = 0;
	int block_id[2] = {-1};
	int block_id_iter = 0;
	GdkPoint tmp_pt;
	int tmp_scale = 0;
	int jump = 1;
	int last_jump = 1;
	int bitmask = ~0x00;
	
	while(TRUE){
		g_mutex_lock(points_updater_mutex);
		while(update_points == FALSE || server_state == BUILD_REQUEST){
			g_cond_wait(points_updater_cond, points_updater_mutex);
		}
		if(structure_changed == TRUE){
			structure_changed = FALSE;
			points_set.clear();
			update_background = TRUE;
			update_screen = TRUE;
			if(storage != NULL){
				delete(storage);
			}
			storage = new disk(BLOCK_SIZE, structure_file_name);
			read_block_id = 0;
			if(storage->read_block(read_block_id, metadata_block)){
				update_points == FALSE;
				continue;
			}
			total_records = *(int*)metadata_block;
			leaf_start_block_id = *(int*)(metadata_block + 4);
			leaf_end_block_id = *(int*)(metadata_block + 8);
			btree_root_block_id = *(int*)(metadata_block + 12);
			offset_x2 = 0;
			offset_y2 = 0;
			block_id_iter = 0;
			for(int i = 0;i < 2;i++){
				x[i] = -1;
				last_x[i] = -1;
				last_y[i] = -1;
				block_id[i] = -1;
			}
		}
		if(update_points == TRUE){
			if(storage == NULL || show_points == FALSE){
				update_points = FALSE;
			}else{
				offset_x2 = offset_x + drawing_area_width * scale;
				offset_y2 = offset_y + drawing_area_height * scale;
				last_x[0] = x[0];
				last_x[1] = x[1];
				x[0] = offset_x;
				x[1] = offset_x2;
				for(int i = 0;i < 2;i++){
					if(last_x[i] == x[i]){
						continue;
					}
					
					read_block_id = btree_root_block_id;
					while(read_block_id > leaf_end_block_id){
						if(storage->read_block(read_block_id, read_block)){
							break;
						}
						
						read_block_cur_pos = read_block;
						
						while(1){
							
							if(*(int*)read_block_cur_pos > x[i] || 
							(read_block_cur_pos + 8) >= (read_block + BLOCK_SIZE) || 
							*(int*)(read_block_cur_pos + 12) == 0){
								read_block_id = *(int*)(read_block_cur_pos + 4);
								break;
							}else{
								read_block_cur_pos += 8;
							}
						}
					}
					block_id[i] = read_block_id;
				}
				if(last_x[0] != offset_x || last_x[1] != offset_x2 || last_y[0] > offset_y || last_y[1] < offset_y2){
					last_y[0] = offset_y;
					last_y[1] = offset_y2;
					block_id_iter = block_id[0];
				}
				if(block_id_iter <= block_id[1]){
					if(storage->read_block(block_id_iter, read_block)){
						break;
					}
					read_block_cur_pos = read_block;
					if(block_id_iter != leaf_end_block_id){
						read_block_end_pos = read_block + BLOCK_SIZE;
					}else{
						read_block_end_pos = read_block + (total_records % (BLOCK_SIZE_8)) * 8;
					}
					tmp_scale = (int)scale;
					jump = 1;
					bitmask = ~0x00;
					while((tmp_scale & bitmask) != 0){
						bitmask <<= 1;
						jump <<= 1;
					}
					jump >>= 1;
					if(last_jump != jump){
						points_set.clear();
						last_jump = jump;
					}
					while(read_block_cur_pos < read_block_end_pos){
						tmp_pt.x = *(int*)read_block_cur_pos;
						tmp_pt.y = *(int*)(read_block_cur_pos + 4);
						read_block_cur_pos += (8 * jump);
						if(tmp_pt.x >= offset_x && tmp_pt.x <= offset_x2 && tmp_pt.y >= offset_y && tmp_pt.y <= offset_y2){
							points_set.insert(tmp_pt);
						}
					}
					block_id_iter++;
				}
				if(block_id_iter > block_id[1]){
					update_points = FALSE;
				}
				update_background = TRUE;
			}
		}
		g_mutex_unlock(points_updater_mutex);
	}
}

/* thread for build/query bcbtree_range_count structure */
gpointer bcbtree_range_count_server(gpointer data){
	gint result = 0;
	gint query_x, query_y;
	gint64 cnt1, cnt2, cnt3, cnt4;
	gint ret_val;
	
	while(TRUE){
		g_mutex_lock(server_mutex);
		while(server_state == NO_REQUEST){
			g_cond_wait(server_cond, server_mutex);
		}
		switch(server_state){
			case BUILD_REQUEST:
				if(source_file_name != NULL && structure_file_name != NULL){
					ret_val = build_BCBtree_range_count(source_file_name, structure_file_name);
					if(ret_val == -1){
						printf("invalid input file\n");
						g_free(source_file_name);
						g_free(structure_file_name);
						source_file_name = NULL;
						structure_file_name = NULL;
						server_state = NO_REQUEST;
						break;
					}
					structure_changed = TRUE;
					update_points = TRUE;
					update_background = TRUE;
					update_screen = TRUE;
				}
				server_state = NO_REQUEST;
				break;
			case QUERY_REQUEST:
				
				server_state = PROCESSING_1;
				break;
			case PROCESSING_1:
				query_x = query_x1;
				query_y = query_y2;
				cnt1 = (gint64) query_BCBtree_range_count(structure_file_name, query_x, query_y);
				
				server_state = PROCESSING_2;
				break;
			case PROCESSING_2:
				query_x = query_x1;
				query_y = query_y1;
				cnt2 = (gint64) query_BCBtree_range_count(structure_file_name, query_x, query_y);
				
				server_state = PROCESSING_3;
				break;
			case PROCESSING_3:
				query_x = query_x2;
				query_y = query_y2;
				cnt3 = (gint64) query_BCBtree_range_count(structure_file_name, query_x, query_y);
				
				server_state = PROCESSING_4;
				break;
			case PROCESSING_4:
				query_x = query_x2;
				query_y = query_y1;
				cnt4 = (gint64) query_BCBtree_range_count(structure_file_name, query_x, query_y);
				query_result = (gint)(cnt3 - cnt1 - cnt4 + cnt2);
				
				server_state = NO_REQUEST;
				update_screen = TRUE;
				break;
			default:
				break;
		}
		
		g_mutex_unlock(server_mutex);
	}
	
	return NULL;
}

void init_all(){
	
	if(!g_thread_supported()){
		g_thread_init(NULL);
	}
	
	server_mutex = g_mutex_new();
	server_cond = g_cond_new();
	points_updater_mutex = g_mutex_new();
	points_updater_cond = g_cond_new();
	
	g_timeout_add(1000 / FRAME_RATE, (GSourceFunc) time_handler, window);
	
	g_thread_create(bcbtree_range_count_server, NULL, FALSE, NULL);
	g_thread_create(points_updater, NULL, FALSE, NULL);
	
	return;
}

int main(int argc, char *argv[]){
	
	/* widget pointers */
	GtkWidget* vbox;
	
    gtk_init(&argc, &argv);
	
	/* window */
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    gtk_window_set_title(GTK_WINDOW(window), "Real Time Range Statistical Explorer");
    gtk_window_set_default_size(GTK_WINDOW(window), 880, 660);
	vbox = createAll();
	gtk_container_add(GTK_CONTAINER(window), vbox);
	
    g_signal_connect(GTK_OBJECT(window), "destroy", G_CALLBACK(gtk_main_quit), NULL);
	
    gtk_widget_show_all(window);
	
	init_all();
	
    gtk_main();
	
	return 0;
}
