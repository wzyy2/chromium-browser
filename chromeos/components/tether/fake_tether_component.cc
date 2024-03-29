// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/fake_tether_component.h"

namespace chromeos {

namespace tether {

FakeTetherComponent::FakeTetherComponent(bool has_asynchronous_shutdown)
    : has_asynchronous_shutdown_(has_asynchronous_shutdown) {}

FakeTetherComponent::~FakeTetherComponent() {}

void FakeTetherComponent::FinishAsynchronousShutdown() {
  DCHECK(status() == TetherComponent::Status::SHUTTING_DOWN);
  TransitionToStatus(TetherComponent::Status::SHUT_DOWN);
}

void FakeTetherComponent::RequestShutdown() {
  DCHECK(status() == TetherComponent::Status::ACTIVE);

  if (has_asynchronous_shutdown_)
    TransitionToStatus(TetherComponent::Status::SHUTTING_DOWN);
  else
    TransitionToStatus(TetherComponent::Status::SHUT_DOWN);
}

}  // namespace tether

}  // namespace chromeos
