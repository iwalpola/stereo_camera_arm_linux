#include <string.h>

#include <gtk/gtk.h>
#include <gst/gst.h>
#include <gst/video/videooverlay.h>

#include <gdk/gdk.h>
#include <gdk/gdkx.h>

/* Structure to contain all our information, so we can pass it around */
typedef struct _CustomData {
  GstElement *pipeline;           /* Our one and only pipeline */
  GstElement *videosource_left, *videosource_right; /* Video source elements */
  GstElement *video_imagesink_left, *video_imagesink_right; /* Video imagesink elements */
  GstElement *video_filesink_left, *video_filesink_right; /* Video filesink elements */
  GstElement *video_tee_left, *video_tee_right; /* Video tee elements */
  GstElement *audiosource_left, *audiosource_right; /* Audio source elements */
  GstElement *audio_interleave, *audio_encode, *audio_filesink; /* Audio processing elements */

  GtkWidget *counter;             /* counter widget to count recording time */
  //GtkWidget *slider;              /* Slider widget to keep track of current position */
  //GtkWidget *streams_list;        /* Text widget to display info about the streams */
  gulong counter_update_signal_id; /* Signal ID for the counter update signal */
  //gulong slider_update_signal_id; /* Signal ID for the slider update signal */

  GstState state;                 /* Current state of the pipeline */
  gint64 maxduration;             /* Maximum duration of the clip, in nanoseconds */
} CustomData;

/* This function is called when the GUI toolkit creates the physical window that will hold the video.
 * At this point we can retrieve its handler (which has a different meaning depending on the windowing system)
 * and pass it to GStreamer through the VideoOverlay interface. */
static void realize_video_left (GtkWidget *leftvideo, CustomData *data) {
  GdkWindow *leftwindow = gtk_widget_get_window (leftvideo);
  guintptr left_window_handle;

  if (!gdk_window_ensure_native (leftwindow))
    g_error ("Couldn't create native window needed for GstVideoOverlay!");

  /* Retrieve window handler from GDK */
  left_window_handle = GDK_WINDOW_XID (leftwindow);
  /* Pass it to playbin, which implements VideoOverlay and will forward it to the video sink */
  gst_video_overlay_set_window_handle (GST_VIDEO_OVERLAY (data->video_imagesink_left), left_window_handle);
  //gst_video_overlay_set_window_handle (GST_VIDEO_OVERLAY (data->playbin), window_handle);
}

static void realize_video_right (GtkWidget *rightvideo, CustomData *data) {
  GdkWindow *rightwindow = gtk_widget_get_window (rightvideo);
  guintptr right_window_handle;

  if (!gdk_window_ensure_native (rightwindow))
    g_error ("Couldn't create native window needed for GstVideoOverlay!");

  /* Retrieve window handler from GDK */
  right_window_handle = GDK_WINDOW_XID (rightwindow);
  /* Pass it to playbin, which implements VideoOverlay and will forward it to the video sink */
  gst_video_overlay_set_window_handle (GST_VIDEO_OVERLAY (data->video_imagesink_right), right_window_handle);
  //gst_video_overlay_set_window_handle (GST_VIDEO_OVERLAY (data->playbin), window_handle);
}

/* This function is called when the RECORD button is clicked */
static void start_record (GtkButton *button, CustomData *data) {
  //create folder
  //set output file locations
  //update state
  gst_element_set_state (data->pipeline, GST_STATE_PLAYING);
}

/* This function is called when the PAUSE button is clicked */
static void pause_record (GtkButton *button, CustomData *data) {
  gst_element_set_state (data->pipeline, GST_STATE_PAUSED);
}

/* This function is called when the STOP button is clicked */
static void stop_record (GtkButton *button, CustomData *data) {
  gst_element_set_state (data->pipeline, GST_STATE_READY);
}

/* This function is called when the main window is closed */
static void delete_event_cb (GtkWidget *widget, GdkEvent *event, CustomData *data) {
  stop_cb (NULL, data);
  gtk_main_quit ();
}

/* This function is called everytime the video window needs to be redrawn (due to damage/exposure,
 * rescaling, etc). GStreamer takes care of this in the PAUSED and PLAYING states, otherwise,
 * we simply draw a black rectangle to avoid garbage showing up. */
static gboolean draw_cb (GtkWidget *widget, cairo_t *cr, CustomData *data) {
  if (data->state < GST_STATE_PAUSED) {
    GtkAllocation allocation;

    /* Cairo is a 2D graphics library which we use here to clean the video window.
     * It is used by GStreamer for other reasons, so it will always be available to us. */
    gtk_widget_get_allocation (widget, &allocation);
    cairo_set_source_rgb (cr, 0, 0, 0);
    cairo_rectangle (cr, 0, 0, allocation.width, allocation.height);
    cairo_fill (cr);
  }

  return FALSE;
}

/* This creates all the GTK+ widgets that compose our application, and registers the callbacks */
static void create_ui (CustomData *data) {
  GtkWidget *main_window;  /* The uppermost window, containing all other windows */
  GtkWidget *video_left; /* The drawing area where the left video will be shown */
  GtkWidget *video_right; /* The drawing area where the left video will be shown */
  GtkWidget *main_box;     /* VBox to hold main_hbox and the controls */
  GtkWidget *main_video_hbox;    /* HBox to hold the video_left and video_right */
  GtkWidget *controls;     /* HBox to hold the buttons and counter */
  GtkWidget *record_button, *pause_button, *stop_button; /* Buttons */

  main_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  g_signal_connect (G_OBJECT (main_window), "delete-event", G_CALLBACK (delete_event_cb), data);

  video_left = gtk_drawing_area_new ();
  video_right = gtk_drawing_area_new ();
  gtk_widget_set_double_buffered (video_left, FALSE);
  gtk_widget_set_double_buffered (video_right, FALSE);
  //listening to event on video_left should be enough to trigger both left and right
  g_signal_connect (video_left, "realize", G_CALLBACK (realize_cb), data);
  g_signal_connect (video_left, "draw", G_CALLBACK (draw_cb), data);

  record_button = gtk_button_new_from_stock (GTK_STOCK_MEDIA_PLAY);
  g_signal_connect (G_OBJECT (record_button), "clicked", G_CALLBACK (start_record), data);

  pause_button = gtk_button_new_from_stock (GTK_STOCK_MEDIA_PAUSE);
  g_signal_connect (G_OBJECT (pause_button), "clicked", G_CALLBACK (pause_record), data);

  stop_button = gtk_button_new_from_stock (GTK_STOCK_MEDIA_STOP);
  g_signal_connect (G_OBJECT (stop_button), "clicked", G_CALLBACK (stop_record), data);

  controls = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_box_pack_start (GTK_BOX (controls), record_button, FALSE, FALSE, 2);
  gtk_box_pack_start (GTK_BOX (controls), pause_button, FALSE, FALSE, 2);
  gtk_box_pack_start (GTK_BOX (controls), stop_button, FALSE, FALSE, 2);

  main_video_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_box_pack_start (GTK_BOX (main_video_hbox), video_left, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (main_video_hbox), video_right, TRUE, TRUE, 0);

  main_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_pack_start (GTK_BOX (main_box), main_video_hbox, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (main_box), controls, FALSE, FALSE, 0);
  gtk_container_add (GTK_CONTAINER (main_window), main_box);
  gtk_window_set_default_size (GTK_WINDOW (main_window), 640, 480);

  gtk_widget_show_all (main_window);
}

/* This function is called when an error message is posted on the bus */
static void error_cb (GstBus *bus, GstMessage *msg, CustomData *data) {
  GError *err;
  gchar *debug_info;

  /* Print error details on the screen */
  gst_message_parse_error (msg, &err, &debug_info);
  g_printerr ("Error received from element %s: %s\n", GST_OBJECT_NAME (msg->src), err->message);
  g_printerr ("Debugging information: %s\n", debug_info ? debug_info : "none");
  g_clear_error (&err);
  g_free (debug_info);

  /* Set the pipeline to READY (which stops playback) */
  gst_element_set_state (data->playbin, GST_STATE_READY);
}

/* This function is called when the pipeline changes states. We use it to
 * keep track of the current state. */
static void state_changed_cb (GstBus *bus, GstMessage *msg, CustomData *data) {
  GstState old_state, new_state, pending_state;
  gst_message_parse_state_changed (msg, &old_state, &new_state, &pending_state);
  if (GST_MESSAGE_SRC (msg) == GST_OBJECT (data->playbin)) {
    data->state = new_state;
    g_print ("State set to %s\n", gst_element_state_get_name (new_state));
    if (old_state == GST_STATE_READY && new_state == GST_STATE_PAUSED) {
      /* For extra responsiveness, we refresh the GUI as soon as we reach the PAUSED state */
      refresh_ui (data);
    }
  }
}

/* This function is called when an "application" message is posted on the bus.
 * Here we retrieve the message posted by the tags_cb callback */
static void application_cb (GstBus *bus, GstMessage *msg, CustomData *data) {
  if (g_strcmp0 (gst_structure_get_name (gst_message_get_structure (msg)), "tags-changed") == 0) {
    /* If the message is the "tags-changed" (only one we are currently issuing), update
     * the stream info GUI */
    analyze_streams (data);
  }
}

int main(int argc, char *argv[]) {
  CustomData data;
  GstStateChangeReturn ret;
  GstBus *bus;

  /* Initialize GTK */
  gtk_init (&argc, &argv);

  /* Initialize GStreamer */
  gst_init (&argc, &argv);

  /* Initialize our data structure */
  memset (&data, 0, sizeof (data));
  //Set max duration by querying memory card space
  //data.maxduration = GST_CLOCK_TIME_NONE;

  /* Create the elements */
  data.videosource_left = gst_element_factory_make ("v4l2src", "videosource_left");
  data.videosource_right = gst_element_factory_make ("v4l2src", "videosource_right");
  data.video_imagesink_left = gst_element_factory_make("ximagesink", "video_imagesink_left");
  data.video_imagesink_right = gst_element_factory_make("ximagesink", "video_imagesink_right");
  data.video_filesink_left = gst_element_factory_make("filesink", "video_filesink_left");
  data.video_filesink_right = gst_element_factory_make("filesink", "video_filesink_right");
  data.video_tee_left = gst_element_factory_make("tee", "video_tee_left");
  data.video_tee_left = gst_element_factory_make("tee", "video_tee_right");
  data.audiosource_left
  data.audiosource_right
  data.audio_interleave

  data.pipeline = gst_pipeline_new("recording_pipeline");

  if (!data.playbin) {
    g_printerr ("Not all elements could be created.\n");
    return -1;
  }

  /* Create the GUI */
  create_ui (&data);

  /* Instruct the bus to emit signals for each received message, and connect to the interesting signals */
  bus = gst_element_get_bus (data.pipeline);
  gst_bus_add_signal_watch (bus);
  g_signal_connect (G_OBJECT (bus), "message::error", (GCallback)error_cb, &data);
  g_signal_connect (G_OBJECT (bus), "message::eos", (GCallback)eos_cb, &data);
  g_signal_connect (G_OBJECT (bus), "message::state-changed", (GCallback)state_changed_cb, &data);
  g_signal_connect (G_OBJECT (bus), "message::application", (GCallback)application_cb, &data);
  gst_object_unref (bus);

  /* Start playing */
  ret = gst_element_set_state (data.playbin, GST_STATE_PLAYING);
  if (ret == GST_STATE_CHANGE_FAILURE) {
    g_printerr ("Unable to set the pipeline to the playing state.\n");
    gst_object_unref (data.playbin);
    return -1;
  }

  /* Register a function that GLib will call every second */
  g_timeout_add_seconds (1, (GSourceFunc)refresh_ui, &data);

  /* Start the GTK main loop. We will not regain control until gtk_main_quit is called. */
  gtk_main ();

  /* Free resources */
  gst_element_set_state (data.playbin, GST_STATE_NULL);
  gst_object_unref (data.playbin);
  return 0;
}