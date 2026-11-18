The moon albedo is NASA's 2048 x 1024 LROC color map from the CGI Moon Kit:
https://svs.gsfc.nasa.gov/4720/

Source: https://svs.gsfc.nasa.gov/vis/a000000/a004700/a004720/lroc_color_2k.jpg
Credit: NASA's Scientific Visualization Studio; Lunar Reconnaissance Orbiter.

The sRGB map is cooked with the existing texture compiler. Its near-side,
projected-area-weighted mean linear luminance is approximately 0.37746.
SkyComponent::moonAlbedo removes that photographic normalization so the
full-moon illumination remains calibrated independently of texture brightness.

The sun, moon, and sky use authored XYZ luminance tables in the renderer's
relative scene units. Moonlight is tuned for the same default exposure as
daylight. The direct lunar profile is cool blue; its Perez sky contribution
uses the night entries of the shared sky table and is captured for IBL.
Moon phase, elevation, and atmospheric attenuation modulate those profiles.

The circular orbit is an approximation, not an astronomical ephemeris. The
phase brightness uses the common lunar magnitude fit 0.026 * abs(alpha) +
4e-9 * alpha^4, with alpha in degrees (Krisciunas and Schaefer, 1991).
The disk uses Lommel-Seeliger reflectance, normalized by its phase integral.
The lunar Perez distribution is an approximation to atmospheric scattering.

References:
https://graphics.stanford.edu/papers/nightsky/nightsky.pdf
https://ui.adsabs.harvard.edu/abs/1991PASP..103.1033K/abstract
