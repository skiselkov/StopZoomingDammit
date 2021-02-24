/*
 * CDDL HEADER START
 *
 * This file and its contents are supplied under the terms of the
 * Common Development and Distribution License ("CDDL"), version 1.0.
 * You may only use this file in accordance with the terms of version
 * 1.0 of the CDDL.
 *
 * A full copy of the text of the CDDL should have accompanied this
 * source.  A copy of the CDDL is also available via the Internet at
 * http://www.illumos.org/license/CDDL.
 *
 * CDDL HEADER END
*/
/*
 * Copyright 2021 Saso Kiselkov. All rights reserved.
 */

#include <stdio.h>
#include <string.h>
#include <time.h>

#include <XPLMCamera.h>
#include <XPLMDisplay.h>
#include <XPLMPlugin.h>
#include <XPLMUtilities.h>

#include <acfutils/assert.h>
#include <acfutils/cmd.h>
#include <acfutils/dr.h>
#include <acfutils/math.h>
#include <acfutils/log.h>
#include <acfutils/time.h>

/*
 * Grand Theory-of-Operation Statement
 *
 * The plugin works using a fairly simple method:
 *
 * 1) Every frame, we register XPLMControlCamera.
 * 2) X-Plane calls our camera control callback.
 * 3) Inside the callback, we grab the current XPLMReadCameraPosition
 *	to figure out the currently applied "zoom" value.
 * 4a) If the zoom value has changed from what we think it should be,
 *	we overwrite the zoom field with the old value and tell X-Plane
 *	that we want to keep control of the camera.
 * 4b) If the zoom value hasn't changed, we tell X-Plane that we are
 *	releasing control of the camera. This makes X-Plane's stock
 *	camera positioning system work (quick looks, hat switches, RMB
 *	tilting, etc.)
 *
 * This approach has the downside that there is some slight jittering
 * when the camera attempts to zoom (because it takes us 1 frame to
 * notice the zoom change), but that is generally a fairly small amount
 * of jitter and can be ignored. It also means, that when X-Plane starts
 * zooming, this inhibits mouse wheel manipulators for about half a
 * second. The user then needs to stop using the mouse wheel for a short
 * while, before attempting to do so. The slight screen judder will let
 * them know intuitively that this is what's happening.
 *
 * Because camera zooming in X-Plane is continuous, we need to allow
 * for a little "grace" period after the zoom-inhibit is re-enagaged.
 * This lets things like quick-look and smooth zooming get finished
 * before we inhibit it again. During this period, the user could use
 * the mouse wheel to zoom accidentally, however, this period is rather
 * short (0.5 - 1.2 seconds), so the chances of that are rather low.
 */

#define	PLUGIN_NAME		"StopZoomingDammit"
#define	PLUGIN_SIG		"skiselkov.stopzoomingdammit"
#define	PLUGIN_DESCRIPTION	"StopZoomingDammit"

static bool_t		allow_zoom = B_FALSE;
static uint64_t		allow_zoom_t = 0;
static bool_t		cam_ctl = B_FALSE;
static double		zoom_tgt = 1.0;

/*
 * Release time of the zoom inhibit function after the custom allow_zoom_*
 * function has been released.
 */
#define	ZOOM_REL_INH_KEY_T	500000	/* us */
/*
 * Release time of the zoom inhibit function after a zoom command
 * (sim/general/zoom_*).
 */
#define	ZOOM_REL_CMD_T		550000	/* us */
/*
 * Release time of the zoom inhibit function after a quick-look command
 * (sim/view/quick_look_*).
 */
#define	ZOOM_REL_QUICK_LOOK_T	1250000	/* us */

static struct {
	dr_t		view_is_ext;
	dr_t		local_vx;
	dr_t		local_vy;
	dr_t		local_vz;
	dr_t		local_ax;
	dr_t		local_ay;
	dr_t		local_az;
	dr_t		P;
	dr_t		Q;
	dr_t		R;
	dr_t		frame_intval;
} drs;

static struct {
	XPLMCommandRef	allow_zoom_hold;
	XPLMCommandRef	allow_zoom_tog;
} cmds;

static int
cam_ctl_cb(XPLMCameraPosition_t *pos, int losing_ctl, void *refcon)
{
	UNUSED(pos);
	UNUSED(losing_ctl);
	UNUSED(refcon);

	if (losing_ctl) {
		cam_ctl = B_FALSE;
		return (0);
	}

	XPLMReadCameraPosition(pos);
	if (allow_zoom || (allow_zoom_t != 0 && microclock() < allow_zoom_t)) {
		/* Learning phase - follow X-Plane's zooming behavior */
		zoom_tgt = pos->zoom;
	} else if (pos->zoom != zoom_tgt) {
		double intval = dr_getf(&drs.frame_intval);
		double vx = dr_getf(&drs.local_vx);
		double vy = dr_getf(&drs.local_vy);
		double vz = dr_getf(&drs.local_vz);
		double ax = dr_getf(&drs.local_ax);
		double ay = dr_getf(&drs.local_ay);
		double az = dr_getf(&drs.local_az);

		/* Inhibit - prevent X-Plane from zooming */
		pos->zoom = zoom_tgt;
		/* Adjust camera to next frame position */
		pos->x += vx * intval + 0.5 * ax * POW2(intval);
		pos->y += vy * intval + 0.5 * ay * POW2(intval);
		pos->z += vz * intval + 0.5 * az * POW2(intval);
		pos->roll += dr_getf(&drs.P) * intval;
		pos->pitch += dr_getf(&drs.Q) * intval;
		pos->heading += dr_getf(&drs.R) * intval;

		allow_zoom_t = 0;

		return (1);
	}

	cam_ctl = B_FALSE;

	return (0);
}

static int
draw_cb(XPLMDrawingPhase phase, int before, void *refcon)
{
	bool_t view_is_ext = (dr_geti(&drs.view_is_ext) != 0);

	UNUSED(phase);
	UNUSED(before);
	UNUSED(refcon);

	if (!view_is_ext && !cam_ctl) {
		XPLMControlCamera(xplm_ControlCameraUntilViewChanges,
		    cam_ctl_cb, NULL);
		cam_ctl = B_TRUE;
	}

	return (1);
}

/*
 * X-Plane sim/view/quick_look_* command handler.
 */
static int
quick_look_cmd_cb(XPLMCommandRef ref, XPLMCommandPhase phase, void *refcon)
{
	UNUSED(ref);
	UNUSED(phase);
	UNUSED(refcon);
	allow_zoom_t = microclock() + ZOOM_REL_QUICK_LOOK_T;
	return (1);
}

/*
 * X-Plane sim/general/zoom_* command handler.
 */
static int
zoom_cmd_cb(XPLMCommandRef ref, XPLMCommandPhase phase, void *refcon)
{
	UNUSED(ref);
	UNUSED(phase);
	UNUSED(refcon);
	allow_zoom_t = microclock() + ZOOM_REL_CMD_T;
	return (1);
}

/*
 * Our custom allow_zoom_* command handler.
 */
static int
allow_zoom_cb(XPLMCommandRef ref, XPLMCommandPhase phase, void *refcon)
{
	UNUSED(refcon);

	if (ref == cmds.allow_zoom_hold) {
		if (phase == xplm_CommandBegin ||
		    phase == xplm_CommandContinue) {
			allow_zoom = B_TRUE;
		} else {
			allow_zoom = B_FALSE;
			allow_zoom_t = microclock() + ZOOM_REL_INH_KEY_T;
		}
	} else if (ref == cmds.allow_zoom_tog) {
		if (phase == xplm_CommandBegin) {
			allow_zoom = !allow_zoom;
			if (!allow_zoom) {
				allow_zoom_t =
				    microclock() + ZOOM_REL_INH_KEY_T;
			}
		}
	}

	return (1);
}

PLUGIN_API int
XPluginStart(char *name, char *sig, char *desc)
{
	/* Always use Unix-native paths on the Mac! */
	XPLMEnableFeature("XPLM_USE_NATIVE_PATHS", 1);
	XPLMEnableFeature("XPLM_USE_NATIVE_WIDGET_WINDOWS", 1);

	log_init(XPLMDebugString, "StopZoomingDammit");
	logMsg("This is StopZoomingDammit");

	strcpy(name, PLUGIN_NAME);
	strcpy(sig, PLUGIN_SIG);
	strcpy(desc, PLUGIN_DESCRIPTION);

	return (1);
}

PLUGIN_API void
XPluginStop(void)
{
}

PLUGIN_API int
XPluginEnable(void)
{
	XPLMRegisterDrawCallback(draw_cb, xplm_Phase_Window, 1, NULL);

	fdr_find(&drs.view_is_ext, "sim/graphics/view/view_is_external");
	fdr_find(&drs.local_vx, "sim/flightmodel/position/local_vx");
	fdr_find(&drs.local_vy, "sim/flightmodel/position/local_vy");
	fdr_find(&drs.local_vz, "sim/flightmodel/position/local_vz");
	fdr_find(&drs.local_ax, "sim/flightmodel/position/local_ax");
	fdr_find(&drs.local_ay, "sim/flightmodel/position/local_ay");
	fdr_find(&drs.local_az, "sim/flightmodel/position/local_az");
	fdr_find(&drs.P, "sim/flightmodel/position/P");
	fdr_find(&drs.Q, "sim/flightmodel/position/Q");
	fdr_find(&drs.R, "sim/flightmodel/position/R");
	fdr_find(&drs.frame_intval, "sim/operation/misc/frame_rate_period");

	for (int i = 0; i < 20; i++) {
		cmd_bind("sim/view/quick_look_%d", quick_look_cmd_cb,
		    B_FALSE, NULL, i);
	}

	cmd_bind("sim/general/zoom_in", zoom_cmd_cb, B_FALSE, NULL);
	cmd_bind("sim/general/zoom_out", zoom_cmd_cb, B_FALSE, NULL);
	cmd_bind("sim/general/zoom_in_fast", zoom_cmd_cb, B_FALSE, NULL);
	cmd_bind("sim/general/zoom_out_fast", zoom_cmd_cb, B_FALSE, NULL);
	cmd_bind("sim/general/zoom_in_slow", zoom_cmd_cb, B_FALSE, NULL);
	cmd_bind("sim/general/zoom_out_slow", zoom_cmd_cb, B_FALSE, NULL);

	cmds.allow_zoom_hold = XPLMCreateCommand("stopzooming/allow_zoom_hold",
	    "Allow zooming while key/button is held");
	VERIFY(cmds.allow_zoom_hold != NULL);
	XPLMRegisterCommandHandler(cmds.allow_zoom_hold, allow_zoom_cb,
	    0, NULL);

	cmds.allow_zoom_tog = XPLMCreateCommand("stopzooming/allow_zoom_toggle",
	    "Toggle allow zooming");
	VERIFY(cmds.allow_zoom_tog != NULL);
	XPLMRegisterCommandHandler(cmds.allow_zoom_tog, allow_zoom_cb, 0, NULL);

	return (1);
}

PLUGIN_API void
XPluginDisable(void)
{
	XPLMUnregisterDrawCallback(draw_cb, xplm_Phase_Window, 1, NULL);

	for (int i = 0; i < 20; i++) {
		cmd_unbind("sim/view/quick_look_%d", quick_look_cmd_cb,
		    B_FALSE, NULL, i);
	}

	cmd_unbind("sim/general/zoom_in", zoom_cmd_cb, B_FALSE, NULL);
	cmd_unbind("sim/general/zoom_out", zoom_cmd_cb, B_FALSE, NULL);
	cmd_unbind("sim/general/zoom_in_fast", zoom_cmd_cb, B_FALSE, NULL);
	cmd_unbind("sim/general/zoom_out_fast", zoom_cmd_cb, B_FALSE, NULL);
	cmd_unbind("sim/general/zoom_in_slow", zoom_cmd_cb, B_FALSE, NULL);
	cmd_unbind("sim/general/zoom_out_slow", zoom_cmd_cb, B_FALSE, NULL);

	XPLMUnregisterCommandHandler(cmds.allow_zoom_hold, allow_zoom_cb,
	    0, NULL);
	XPLMUnregisterCommandHandler(cmds.allow_zoom_tog, allow_zoom_cb,
	    0, NULL);
}

PLUGIN_API void
XPluginReceiveMessage(XPLMPluginID from, int msg, void *param)
{
	UNUSED(from);
	UNUSED(msg);
	UNUSED(param);
}
