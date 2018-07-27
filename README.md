# OTA for Linux things

## Brief

Implementation of a small full image active/inactive OTA system for automatically
updating Linux things.

## Mechanism

Two complete copies of your firmware are stored somewhere on your thing.
When uboot starts it loads both of the images in to RAM and validates them.
Unless it's told otherwise it boots the newest image.

Updating the firmware is then just a case of replacing the inactive copy
with the new version of the firmware and rebooting. Uboot tells the kernel
which copy it booted so that the OTA application knows which copy should
be replaced.

## Requirements

### Software
* uboot
* thingymcconfig
* your firmware as a FIT image

### Hardware
* Enough storage for your firmware image twice, uboot and any other bits you need.
* Enough RAM to hold the two images during boot up.
* A GPIO to trigger booting the previous firmware in recovery situations.

## Firmware repo

### Layout
```
manifest.json
sig.json
myimage_0.fit
myimage_1.fit
...
```

### Manifest format

```json
{
	"serial": 0,
	"images": [
		{
			"uuid": "8ec32fe0-9bff-44bd-9f84-0b63088b1f13",
			"version": 0,
			"size": 0,
			"enabled": true,
			"tags": [
			],
			"signatures": [
				{
					"type": "rsa-sha256",
					"data": "xxx"
				}
			]
		}
	]
}
```

```json
[
	{
		"type": "rsa-sha256",
		"data": "xxx"
	}
]
