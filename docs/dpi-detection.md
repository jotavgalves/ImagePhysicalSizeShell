# DPI Detection

The project must calculate centimeters only from explicit physical metadata.

## PNG

The Microsoft WIC native metadata query table documents many PNG chunks but does not list `pHYs` as a queryable path in the current documentation. Because `pHYs` is the required physical-resolution signal for PNG, the project performs a direct, bounded PNG chunk scan to confirm whether `pHYs` exists. The unit byte must represent meters. Convert pixels per meter to DPI with:

```text
dpi = pixelsPerMeter * 0.0254
```

If `pHYs` is absent or unitless, DPI is absent.

## JPEG

JPEG must inspect both JFIF APP0 and EXIF metadata.

WIC documented paths used for later COM integration and cross-checking:

- `/app0/{ushort=1}`: JFIF units;
- `/app0/{ushort=2}`: JFIF X density;
- `/app0/{ushort=3}`: JFIF Y density;
- `/app1/ifd/{ushort=282}`: EXIF XResolution;
- `/app1/ifd/{ushort=283}`: EXIF YResolution;
- `/app1/ifd/{ushort=296}`: EXIF ResolutionUnit.

JFIF unit handling:

- `0`: aspect ratio only, not physical size;
- `1`: pixels per inch;
- `2`: pixels per centimeter, convert to DPI by multiplying by `2.54`.

EXIF handling:

- read `XResolution`;
- read `YResolution`;
- read `ResolutionUnit`;
- accept inch and centimeter units only.

## TIFF

TIFF uses rational `XResolution` and `YResolution` plus `ResolutionUnit`. The first frame/page is the reference for version 1.

WIC documented paths used for later COM integration and cross-checking:

- `/ifd/{ushort=282}`: XResolution;
- `/ifd/{ushort=283}`: YResolution;
- `/ifd/{ushort=296}`: ResolutionUnit.

## Validation

Reject zero, negative, NaN, infinity and unreasonable DPI values. No fallback DPI is allowed.
