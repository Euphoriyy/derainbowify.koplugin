# derainbowify.koplugin

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)
[![GitHub Release](https://img.shields.io/github/v/release/Euphoriyy/derainbowify.koplugin)](https://github.com/Euphoriyy/derainbowify.koplugin/releases)
[![Lua](https://img.shields.io/badge/Lua-%232C2D72.svg?logo=lua&logoColor=white)](#)
[![C](https://img.shields.io/badge/C-00599C?logo=c&logoColor=white)](#)
[![C++](https://img.shields.io/badge/C%2B%2B-00599C?style=flat&logo=c%2B%2B&logoColor=white)](#)
[![GitHub Downloads](https://img.shields.io/github/downloads/Euphoriyy/derainbowify.koplugin/total.svg)](https://github.com/Euphoriyy/derainbowify.koplugin/releases)

<img src="img/option.png" style="width:100%; height:auto;">

This plugin adds an option to remove the rainbow artifacts produced on Kaleido 3 color e-ink screens. It is useful for black and white manga (and other comics) that use image patterns which produce these artifacts.

It uses fourier transforms on-the-fly to directly target the moiré patterns responsible for the rainbow effect.
Compared to the method used for reducing the rainbow effect that is found in the official Kobo software, the method utilized by this plugin does *not* negatively affect the image by introducing blur, instead retaining sharpness.

The methodology for this is explained in detail by [this video](https://www.youtube.com/watch?v=Dw2HTJCGMhw) by [@axu2](https://github.com/axu2), the maintainer of [Kindle Comic Converter](https://github.com/ciromattia/kcc).

Currently the filtering does make rendering pages for the first time slower, but the goal is to gradually make improvements to the performance.

Made for koreader/koreader#11877.

## Installation
1. Download the latest release.
2. Extract the `derainbowify.koplugin` directory from the archive.
3. Move the `derainbowify.koplugin` directory to the `koreader/plugins` folder on your device.
4. Restart KOReader.
5. You should see a new option called "Derainbow" on the contrast menu when opening a document.

## Compatibility
By design, this plugin is designed for e-readers that use color e-ink such as the Kobo Libra Color, Kindle Colorsoft, and Pocketbook Inkpad Color 3.

For now, only the Kobo Libra Color has been fully tested, but the other devices are supported.

## Building
In order to compile and test the plugin yourself, you may run the `package.sh` script found at the root of this repository. It will produce a ZIP file containing the plugin located at `/tmp/derainbowify.koplugin.zip`.

The dependencies for compilation are `make`, `gcc`, and each of the following toolchains found from KOReader's [koxtoolchain](https://github.com/koreader/koxtoolchain/releases):
- Kobo
- Kindle5
- KindleHF
- Pocketbook

Please ensure you have the necessary toolchains added to your path.

## Acknowledgements
- Adapted from the original amazing work done by [@Its-my-right](https://github.com/Its-my-right) for the Pocketbook Inkpad Color 3 [here](https://github.com/Its-my-right/pocketbook_cfa_interference_breaker).
- Based on the technique used in [Kindle Comic Converter](https://github.com/ciromattia/kcc) and the great video presentation by [@axu2](https://github.com/axu2).
- Inspired by the idea from [@Blendman974](https://github.com/Blendman974) to use fast fourier transforms to solve this issue.
- This plugin uses the following libraries for fast fourier transform implementations: [kissfft](https://github.com/mborgerding/kissfft), [pffft](https://bitbucket.org/jpommier/pffft), and [pocketfft](https://github.com/hayguen/pocketfft).
- The packaging system in use is based on work from [ComicReader](https://github.com/KORComic/comicreader.koplugin) by [@OGKevin](https://github.com/OGKevin).

## Support
If you want to support me, you can star this repository or buy me a coffee. :)

[![ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/X8X21UH4E1)

## License
This project is licensed under the **GNU General Public License v3.0**.
See the [LICENSE](./LICENSE) file for full details.
