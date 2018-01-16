// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_tiles/ntp_tile_impression.h"

namespace ntp_tiles {

NTPTileImpression::NTPTileImpression()
    : NTPTileImpression(/*index=*/0,
                        /*source=*/TileSource::TOP_SITES,
                        /*title_source=*/TileTitleSource::UNKNOWN,
                        /*visual_type=*/TileVisualType::UNKNOWN_TILE_TYPE,
                        /*data_generation_time=*/base::Time(),
                        /*url_for_rappor=*/GURL()) {}

NTPTileImpression::NTPTileImpression(int index,
                                     TileSource source,
                                     TileTitleSource title_source,
                                     TileVisualType visual_type,
                                     base::Time data_generation_time,
                                     const GURL& url_for_rappor)
    : index(index),
      source(source),
      title_source(title_source),
      visual_type(visual_type),
      data_generation_time(data_generation_time),
      url_for_rappor(url_for_rappor) {}

NTPTileImpression::~NTPTileImpression() {}

}  // namespace ntp_tiles