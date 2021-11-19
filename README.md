About
---------

wxOpenCVCameras is a crude untested example of how to retrieve and display
images from multiple cameras, using OpenCV to grab images from a camera
and wxWidgets to display the images.

Every camera has its own worker thread `CameraThread`, which grabs a frame
from a camera with `cv::VideoCapture`, converts the frame from `cv::Mat`
to `wxBitmap`, and creates a resized thumbnail also converted to `wxBitmap`.
Additionally, benchmarking data (times for grabbing image, converting
it to `wxBitmap`, creating a thumbnail...) are collected.

The full resolution bitmap, the thumbnail bitmap, and the benchmarking data
are stored  in `CameraFrameData` class. As `wxBitmap` is implemented as
copy-on-write in a probably thread-unsafe manner, the `wxBitmap`s are stored
as raw pointers and `CameraFrameData`s in `std::unique_ptr`.

`CameraFrameData` is then added by a worker thread to a container
shared between the GUI thread and camera threads. The container is `std::vector`
(see `CameraGridFrame::m_newCameraFrameData` and `CameraThread::m_frames`) protected
by `wxCriticalSection` (see `CameraGridFrame::m_newCameraFrameDataCS`
and `CameraThread::m_framesCS`). The GUI thread (a `wxFrame`-derived `CameraGridFrame`)
then uses a fixed-frequency `wxTimer` to update the camera display with images stored
in the container.

GUI
---------
Output from all cameras is displayed in a single frame as thumbnails (`CameraPanel`s
in a `wxWrapSizer`), left doubleclicking a thumbnail opens a new frame (`OneCameraFrame`)
showing the camera output in the full resolution.

A camera can be added either as an integer (e.g., `0` for a default webcam) or as an URL.
There is a couple of preset camera URLs offered via a menu, but these are not guaranteed
to stay online and are mostly time-limited.

In the debug build, various diagnostic messages are output with `wxLogTrace(TRACE_WXOPENCVCAMERAS, ...)`.

Note
---------
Removing a camera (i.e., stopping a thread) may sometimes take a while so that the program
appears to be stuck. However, this happens when the worker thread is stuck in an OpenCV call
(e.g., opening/closing `cv::VideoCapture` or grabbing the image) that may sometimes take a while,
where the thread cannot test whether to stop.

Requirements
---------
Requires OpenCV v4.2+, wxWidgets v3.1+, and a compiler supporting C++11 standard.

Licence
---------
[wxWidgets licence](https://github.com/wxWidgets/wxWidgets/blob/master/docs/licence.txt)