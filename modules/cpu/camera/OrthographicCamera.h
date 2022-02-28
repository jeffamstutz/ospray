// Copyright 2009-2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "camera/Camera.h"
// ispc shared
#include "OrthographicCameraShared.h"

namespace ospray {

// The Orthographic Camera ("orthographic")
//
// Implements a straightforward orthographic camera for orthographic
// projections, without support for Depth of Field
//
// A simple orthographic camera. This camera type is loaded by passing
// the type string "orthographic" to ospNewCamera
//
// The functionality for a orthographic camera is implemented via the
// ospray::OrthographicCamera class.
struct OSPRAY_SDK_INTERFACE OrthographicCamera
    : public AddStructShared<Camera, ispc::OrthographicCamera>
{
  OrthographicCamera();
  ~OrthographicCamera() override = default;

  virtual std::string toString() const override;
  virtual void commit() override;

  box3f projectBox(const box3f &b) const override;

  // Data members //

  float height; // size of the camera's image plane in y, in world coordinates
  float aspect;
};

} // namespace ospray
