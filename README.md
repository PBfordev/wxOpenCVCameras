About
---------

wxOpenCVCameras is a crude untested example of how to retrieve and display
images from multiple cameras, using OpenCV to grab images from a camera
and wxWidgets to display the images.

Every camera has its own worker thread (probably not the optimal solution),
which grabs a frame from a camera with cv::VideoCapture, 
converts the frame from cv::Mat to wxBitmap, and creates a resized
thumbnail also converted to wxBitmap. Additionally, benchmarking data
(times for grabbing image, converting it to wxBitmap, and others) are collected.

A full-sized frame, a thumbnail, and benchmarking data are then added
by a camera thread to a container shared between the GUI thread and camera threads.
The GUI thread (a wxFrame) then uses a fixed-frequency timer to update the camera
display with images stored in the container.

GUI
---------
Output from all cameras is displayed in a single frame as thumbnails (wxPanels added
in a wxWrapSizer), left doubleclicking a thumbnail opens a new frame showing the
camera output in full resolution.

A camera can be added either as an integer (e.g., 0 for a default webcam) or as an URL.
There is a couple of preset camera URLs offered via a menu, but these are not guaranteed
to stay online and are mostly time-limited.

Note
---------
Removing a camera (i.e., stopping a thread) may sometimes take a while so that the program 
appears to be stuck. However, this happens when the cv::VideoCapture is still being opened, 
during which the thread cannot test whether to quit.

Requirements
---------
Requires OpenCV 4.2+, wxWidgets 3.1+ and a compiler supporting C++11 standard.

Licence
---------
[wxWidgets licence](https://github.com/wxWidgets/wxWidgets/blob/master/docs/licence.txt)