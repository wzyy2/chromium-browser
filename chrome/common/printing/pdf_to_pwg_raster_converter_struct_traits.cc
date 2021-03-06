// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/printing/pdf_to_pwg_raster_converter_struct_traits.h"

#include "ui/gfx/geometry/mojo/geometry_struct_traits.h"

namespace mojo {

// static
bool StructTraits<printing::mojom::PDFRenderSettingsDataView,
                  printing::PdfRenderSettings>::
    Read(printing::mojom::PDFRenderSettingsDataView data,
         printing::PdfRenderSettings* out) {
  out->dpi = data.dpi();
  out->autorotate = data.autorotate();

  return data.ReadArea(&out->area) && data.ReadOffsets(&out->offsets) &&
         data.ReadMode(&out->mode);
}

bool StructTraits<printing::mojom::PWGRasterSettingsDataView,
                  printing::PwgRasterSettings>::
    Read(printing::mojom::PWGRasterSettingsDataView data,
         printing::PwgRasterSettings* out) {
  out->rotate_all_pages = data.rotate_all_pages();
  out->reverse_page_order = data.reverse_page_order();
  return data.ReadOddPageTransform(&out->odd_page_transform);
}

}  // namespace mojo
