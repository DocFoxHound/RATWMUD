# Wolf Doll Assets (`Data/Wolves/assets/`)

Canonical repository layout for all six wolf dolls (`female-small`, `female-medium`, `female-large`, `male-small`, `male-medium`, `male-large`) plus shared face art.

## Directory Layout

```text
Data/Wolves/assets/
├── README.md
├── manifest.json                       # Machine-readable index of all available assets
├── faces/                              # Shared face art / expression templates (hand-drawn)
└── <sex>-<size>/                       # e.g. female-medium (Average Female)
    ├── poses/                          # Base neutral 48-layer Porter-Duff .zip dolls
    │   ├── standing.zip
    │   ├── lying.zip
    │   ├── sleeping.zip
    │   ├── knocked_out.zip
    │   ├── roll.zip
    │   ├── playful.zip
    │   ├── sitting.zip
    │   ├── stalking.zip
    │   ├── walking.zip
    │   ├── jogging.zip
    │   ├── running.zip
    │   └── sprinting.zip
    ├── expressions/                    # Standalone 48-layer .zip dolls per <pose>/<expression>.zip
    │   ├── standing/{angry,angry_scared,angry_dominant,threatening,sad,happy,hurt,tired,surprised}.zip
    │   ├── sitting/{angry,angry_scared,angry_dominant,threatening,sad,happy,hurt,tired,surprised}.zip
    │   ├── lying/{angry,angry_scared,angry_dominant,threatening,sad,happy,hurt,tired,surprised}.zip
    │   └── {walking,jogging,running,sprinting}/{angry,scared}.zip
    ├── damage/                         # Transparent 1024x1024 RGBA overlay PNGs per <pose>/<region>.png
    │   └── <pose>/{head,neck,forelegs,hindlegs,torso,tail}.png
    ├── anatomy/                        # Clipped ears & tail variants per <pose>/
    │   └── <pose>/
    │       ├── clipped_ear_left_{erase,overlay}.png
    │       ├── clipped_ear_right_{erase,overlay}.png
    │       ├── clipped_ear_both_{erase,overlay}.png
    │       ├── docked_tail.zip         # 48-layer doll with docked tail
    │       └── half_limp_tail.zip      # 48-layer doll with half-limp tail
    ├── markings/                       # Colourable white+alpha 1024x1024 overlay PNGs per <pose>/<marking>.png
    │   └── <pose>/{brindle,merle,piebald,ticking,irish_saddle,frosting}.png
    ├── wearables/                      # Transparent 1024x1024 RGBA gear overlays per <pose>/<item>.png
    └── faces/                          # Per-doll hand-drawn faces & expressions
```

## 48-Layer Doll Schema (`.zip`)

Every standalone `.zip` under `poses/`, `expressions/`, and tail `anatomy/` contains **48 numbered `1024×1024` RGBA PNGs** stacked from `01` (lowest) to `48` (highest) using standard Porter-Duff `source-over` compositing (with layer `47` multiply-blended):

- `01`: `EYES` (`#f2d97e`)
- `02`–`07`: Tail (`TAIL_TIP`, `TAIL_LWR`, `TAIL_MID`, `TAIL_UPPR`, `TAIL_ACCENT`, `TAIL_TOP`)
- `08`–`12`: Torso (`BELLY`, `RIBS_LWR`, `RIBS_MID`, `RIBS_UPPR`, `BACK_ACCENT`)
- `13`–`20`: Hind legs & haunches (`LEGS_ACCENT_REAR`, `LEGS_REAR`, `HAUNCHES_LWR`, `HAUNCHES_UPPR`, `HAUNCHES_TOP`, `TOES_REAR`, `SOCKS_REAR`, `CLAWS_REAR`)
- `21`–`28`: Front legs & shoulders (`LEGS_ACCENT_FRONT`, `LEGS_FRONT`, `SHOULDER_LWR`, `SHOULDER_UPPR`, `SHOULDER_TOP`, `TOES_FRONT`, `SOCKS_FRONT`, `CLAWS_FRONT`)
- `29`–`33`: Neck (`NECK_UNDER`, `NECK_1`, `NECK_2`, `NECK_3`, `TOP`)
- `34`–`46`: Head & ears (`CHIN_JAW`, `CHEEKS_FRONT`, `CHEEKS_ACCENT`, `CHEEKS`, `UPR_HEAD`, `HEAD_ACCENTS`, `MUZZLE`, `UPR_EYE`, `LWR_EYE`, `INNR_EAR`, `UPR_EAR`, `LWR_EAR`, `EYEBROW`)
- `47`: `SHADOW` (neutral grey `rgb(131,131,131)` multiply shading)
- `48`: `LINEART` (pure black ink + white eye/nose catchlights)

All 6 flat base colours match `Female_Average.zip` (`#302921`, `#665747`, `#877360`, `#ab9178`, `#f2e3d4`, `#f2d97e`), and lower layers are filled solid beneath upper layers so standard `source-over` compositing produces zero translucent seams.

## Overlay Layers

- **Markings (`markings/<pose>/<id>.png`)**: White (`#FFFFFF`) pixels with hand-brushed fur-tuft alpha, clipped strictly to the fur silhouette (excluding eyes and claws). Tint to any palette colour and composite **above layer 46 and below layers 47 (`SHADOW`) and 48 (`LINEART`)**.
- **Damage (`damage/<pose>/<region>.png`)**: Full-colour transparent RGBA overlays composited **above layer 48 (`LINEART`)** so any combination of injuries stacks cleanly over any coat.
- **Clipped Ears (`anatomy/<pose>/clipped_ear_<side>_{erase,overlay}.png`)**: Apply `destination-out` with `*_erase.png` on the doll, then `source-over` `*_overlay.png` for the healed scar notch lineart.
- **Wearables (`wearables/<pose>/<item>.png`)**: Full-colour transparent RGBA overlays aligned 1:1 with the pose's `1024×1024` canvas, composited above layer 48.
