// Copyright 2009-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "Camera.ih"
#include "common/MotionTransform.ih"
#include "math/sampling.ih"
#include "ospray/OSPEnums.h"

struct PerspectiveCamera
{
  Camera super;

  vec3f org; // position of camera, already contains shift when
             // STEREO_{LEFT|RIGHT}
  vec3f dir_00; // direction of ray with screenSample=(0,0); scaled to
                // focusDistance if no motionBlur
  // below are essentially unions: 1. if no motionBlur; 2. if motionBlur
  vec3f du_size; // delta of ray direction between two pixels in x, scaled to
                 // focusDistance; sensor size (in x and y) and architectural
  vec3f dv_up; // delta of ray direction between two pixels in y, scaled to
               // focusDistance; up direction of camera
  vec3f ipd_offset; // shift of camera position for left/right eye (only when
                    // SIDE_BY_SIDE or TOP_BOTTOM); (0.5*ipd, focusDistance, 0)

  float scaledAperture; // radius of aperture prescaled to focal plane, i.e.,
                        // divided by horizontal image plane size
  float aspect; // image plane size x / y
  int stereoMode;
};

void PerspectiveCamera_initRay(const Camera *uniform _self,
    varying Ray &ray,
    const varying CameraSample &sample)
{
  const PerspectiveCamera *uniform self =
      (const PerspectiveCamera *uniform)_self;

  vec2f screen = sample.screen;
  vec3f org = self->org;

  const uniform bool sbs = self->stereoMode == OSP_STEREO_SIDE_BY_SIDE;
  varying float *uniform split = sbs ? &screen.x : &screen.y;
  if (or(sbs, self->stereoMode == OSP_STEREO_TOP_BOTTOM)) {
    *split *= 2.f;
    if (*split < 1.f) {
      org = org - self->ipd_offset;
    } else {
      org = org + self->ipd_offset;
      *split -= 1.f;
    }
  }

  screen = Camera_subRegion(_self, screen);

  vec3f dir = self->dir_00 + screen.x * self->du_size + screen.y * self->dv_up;

  if (self->scaledAperture > 0.f) {
    const vec3f llp = uniformSampleDisk(self->scaledAperture, sample.lens);
    // transform local lens point to focal plane (dir_XX are prescaled)
    const vec3f lp =
        (llp.x * self->du_size) + ((llp.y * self->aspect) * self->dv_up);
    org = org + lp;
    dir = dir - lp;
  }

  const float time = Camera_shutterTime(_self, sample.time);
  setRay(ray, org, normalize(dir), self->super.nearClip, inf, time);
}

void PerspectiveCamera_initRayMB(const Camera *uniform _self,
    varying Ray &ray,
    const varying CameraSample &sample)
{
  const PerspectiveCamera *uniform self =
      (const PerspectiveCamera *uniform)_self;

  const float time = Camera_shutterTime(_self, sample.time);
  const affine3f xfm = getInterpolatedTransform(self->super.geom, time);

  vec3f org = xfmPoint(xfm, self->org);
  vec3f dir = normalize(xfmVector(xfm, self->dir_00));
  const vec3f up = xfmVector(xfm, self->dv_up);
  vec3f du = normalize(cross(dir, up));
  vec3f dv;
  if (self->du_size.z > 0.f) // architectural: orient img to be parallel to 'up'
    dv = normalize(up);
  else // rotate film to be perpendicular to 'dir'
    dv = cross(du, dir);

  vec3f ipd_offset = self->ipd_offset.x * du;

  switch (self->stereoMode) {
  case OSP_STEREO_LEFT:
    org = org - ipd_offset;
    break;
  case OSP_STEREO_RIGHT:
    org = org + ipd_offset;
    break;
  case OSP_STEREO_TOP_BOTTOM:
    // flip offset to have left eye at top (image coord origin at lower left)
    ipd_offset = neg(ipd_offset);
    break;
  default:
    break;
  }

  du = du * self->du_size.x;
  dv = dv * self->du_size.y;
  dir = dir - 0.5f * du - 0.5f * dv;

  // prescale to focal plane
  if (self->scaledAperture > 0.f) {
    du = du * self->ipd_offset.y; // focusDistance
    dv = dv * self->ipd_offset.y; // focusDistance
    dir = dir * self->ipd_offset.y; // focusDistance
  }

  vec2f screen = sample.screen;

  const uniform bool sbs = self->stereoMode == OSP_STEREO_SIDE_BY_SIDE;
  varying float *uniform split = sbs ? &screen.x : &screen.y;
  if (or (sbs, self->stereoMode == OSP_STEREO_TOP_BOTTOM)) {
    *split *= 2.f;
    if (*split < 1.f) {
      org = org - ipd_offset;
    } else {
      org = org + ipd_offset;
      *split -= 1.f;
    }
  }

  screen = Camera_subRegion(_self, screen);

  dir = dir + screen.x * du + screen.y * dv;

  if (self->scaledAperture > 0.f) {
    const vec3f llp = uniformSampleDisk(self->scaledAperture, sample.lens);
    // transform local lens point to focal plane (dir_XX are prescaled)
    const vec3f lp = (llp.x * du) + ((llp.y * self->aspect) * dv);
    org = org + lp;
    dir = dir - lp;
  }

  setRay(ray, org, normalize(dir), self->super.nearClip, inf, time);
}

export void *uniform PerspectiveCamera_create(void *uniform cppE)
{
  uniform PerspectiveCamera *uniform self =
      uniform new uniform PerspectiveCamera;
  self->super.cppEquivalent = cppE;
  self->super.initRay = PerspectiveCamera_initRay;
  return self;
}

export void PerspectiveCamera_set(void *uniform _self,
    const uniform vec3f &org,
    const uniform vec3f &dir,
    const uniform vec3f &up,
    const uniform vec2f &imgPlaneSize,
    const uniform float scaledAperture,
    const uniform float focusDistance,
    const uniform float aspect,
    const uniform bool architectural,
    const uniform int stereoMode,
    const uniform float ipd)
{
  PerspectiveCamera *uniform self = (PerspectiveCamera * uniform) _self;

  self->scaledAperture = scaledAperture;
  self->aspect = aspect;
  self->stereoMode = stereoMode;
  self->dir_00 = normalize(dir);
  self->org = org;

  if (self->super.motionBlur) {
    self->super.initRay = PerspectiveCamera_initRayMB;
    self->du_size = make_vec3f(imgPlaneSize.x, imgPlaneSize.y, architectural);
    self->dv_up = up;
    self->ipd_offset = make_vec3f(0.5f * ipd, focusDistance, 0.0f);
  } else {
    self->super.initRay = PerspectiveCamera_initRay;
    self->du_size = normalize(cross(self->dir_00, up));
    if (architectural) // orient film to be parallel to 'up'
      self->dv_up = normalize(up);
    else // rotate film to be perpendicular to 'dir'
      self->dv_up = cross(self->du_size, self->dir_00);

    self->ipd_offset = 0.5f * ipd * self->du_size;

    switch (stereoMode) {
    case OSP_STEREO_LEFT:
      self->org = self->org - self->ipd_offset;
      break;
    case OSP_STEREO_RIGHT:
      self->org = self->org + self->ipd_offset;
      break;
    case OSP_STEREO_TOP_BOTTOM:
      // flip offset to have left eye at top (image coord origin at lower left)
      self->ipd_offset = neg(self->ipd_offset);
      break;
    default:
      break;
    }

    self->du_size = self->du_size * imgPlaneSize.x;
    self->dv_up = self->dv_up * imgPlaneSize.y;
    self->dir_00 = self->dir_00 - 0.5f * self->du_size - 0.5f * self->dv_up;

    // prescale to focal plane
    if (scaledAperture > 0.f) {
      self->du_size = self->du_size * focusDistance;
      self->dv_up = self->dv_up * focusDistance;
      self->dir_00 = self->dir_00 * focusDistance;
    }
  }
}
