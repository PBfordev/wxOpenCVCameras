About
---------

wxOpenCVCameras is a crude untested example of how to retrieve and display
images from multiple cameras, using OpenCV to grab images from a camera
and wxWidgets to display the images.

![wxOpenCVCameras Screenshot](screenshots/wxopencvcameras.png?raw=true)

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
(see `CameraGridFrame::m_newCameraFrameData` and `CameraThread::CameraSetupData::frames`) protected
by `wxCriticalSection` (see `CameraGridFrame::m_newCameraFrameDataCS`
and `CameraThread::CameraSetupData::framesCS`). The GUI thread (a `wxFrame`-derived `CameraGridFrame`)
then uses a fixed-frequency `wxTimer` to update the camera display with images stored
in the container.

The GUI has a crude control of the camera (thread) by using `wxMessageQueue` to pass
the commands (such as setting the thread sleep time or getting/setting one of
`cv::VideoCaptureProperties`) from the GUI to the camera thread.

GUI
---------
A camera can be added either as an integer (e.g., `0` for a default webcam) or as an URL.
There is a couple of preset camera URLs offered via a menu, but these are not guaranteed
to stay online and are mostly time-limited. When a camera is added, settings from menu
"Defaults for New Cameras" are used.

Output from all cameras is displayed in a single frame as thumbnails (`CameraPanel`s
in a `wxWrapSizer`). Left doubleclicking a thumbnail opens a new frame (`OneCameraFrame`)
showing the camera output in the full resolution. Right clicking a thumbnail shows 
a popup menu allowing crude communication with the camera (thread).

In the debug build, various diagnostic messages are output with `wxLogTrace(TRACE_WXOPENCVCAMERAS, ...)`.

Notes
---------
wxOpenCVCameras uses internet streams as camera sources. If an application connects to multiple
hardware cameras with the same parameters, an entirely different approach should probably be used.
For example, a single worker thread for capturing from all the cameras, using `cv::VideoCapture::waitAny()`
with `cv::VideoCapture::retrieve()` and one or more worker threads for processing the captured
images. Such approach would be not only less thread-hungry, the frames from multiple cameras
should be better synchronized as well.

To convert `cv::Mat` to `wxBitmap`, the code uses `ConvertMatBitmapTowxBitmap()` from the
[wxOpenCVTest project](https://github.com/PBfordev/wxopencvtest), so all the information
provided there applies here is as well.

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