# StopZoomingDammit

This is a small plugin for X-Plane that temporarily inhibits the X-Plane
mouse-wheel-to-zoom function.

[![Donate](https://img.shields.io/badge/Donate-PayPal-green.svg)](https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=8DN9LYD5VP4NY)

## What is the purpose of this plugin

When using the mouse wheel to adjust a control in the cockpit, it's
common to accidentally "slip off" the control while spinning the mouse
wheel. In that case, X-Plane will start zooming the view in or out and
this can be extremely annoying. This plugin prevents view zooming using
the mouse wheel. You can define a joystick button or keyboard key to
temporarily enable mouse wheel zoomming. Simply assigning one of the
following commands to your controls of choice:

* ``stopzooming/allow_zoom_hold`` - while holding down this command, you
will be able to zoom using the mouse wheel. When you release the command,
mouse-wheel-zoom will be inhibited again.

* ``stopzooming/allow_zoom_toggle`` - trigger this command to toggle
between enabling and disable mouse wheel zoom.

The plugin doesn't affect joystick or keyboard zoom commands
(``sim/general/zoom_*``) as well as zooming due to different view presets
(``sim/view/quick_look_*`` or the quick-look functions using NumPad
keys). Those remain fully functional.

## Installation

Simply unzip the plugin and drop it under ``Resources/plugins`` in
X-Plane. Then define your mouse-zoom-allow function as described above
and that's it. There is no further configuration required.

## Compatibility

The plugin should work with any X-Plane aircraft. However, it might
conflict with camera-control plugins such as X-Camera. The plugin has
only been tested with the stock X-Plane camera system.
