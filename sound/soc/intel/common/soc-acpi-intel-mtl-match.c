// SPDX-License-Identifier: GPL-2.0-only
/*
 * soc-acpi-intel-mtl-match.c - tables and support for MTL ACPI enumeration.
 *
 * Copyright (c) 2022, Intel Corporation.
 *
 */

#include <sound/soc-acpi.h>
#include <sound/soc-acpi-intel-match.h>
#include "soc-acpi-intel-sdw-mockup-match.h"

static const struct snd_soc_acpi_codecs mtl_max98357a_amp = {
	.num_codecs = 1,
	.codecs = {"MX98357A"}
};

static const struct snd_soc_acpi_codecs mtl_rt5682_rt5682s_hp = {
	.num_codecs = 2,
	.codecs = {"10EC5682", "RTL5682"},
};

struct snd_soc_acpi_mach snd_soc_acpi_intel_mtl_machines[] = {
	{
		.comp_ids = &mtl_rt5682_rt5682s_hp,
		.drv_name = "mtl_mx98357_rt5682",
		.machine_quirk = snd_soc_acpi_codec_list,
		.quirk_data = &mtl_max98357a_amp,
		.sof_tplg_filename = "sof-mtl-max98357a-rt5682.tplg",
	},
	{},
};
EXPORT_SYMBOL_GPL(snd_soc_acpi_intel_mtl_machines);

/* this table is used when there is no I2S codec present */
struct snd_soc_acpi_mach snd_soc_acpi_intel_mtl_sdw_machines[] = {
	/* mockup tests need to be first */
	{
		.link_mask = GENMASK(3, 0),
		.links = sdw_mockup_headset_2amps_mic,
		.drv_name = "sof_sdw",
		.sof_tplg_filename = "sof-mtl-rt711-rt1308-rt715.tplg",
	},
	{
		.link_mask = BIT(0) | BIT(1) | BIT(3),
		.links = sdw_mockup_headset_1amp_mic,
		.drv_name = "sof_sdw",
		.sof_tplg_filename = "sof-mtl-rt711-rt1308-mono-rt715.tplg",
	},
	{
		.link_mask = GENMASK(2, 0),
		.links = sdw_mockup_mic_headset_1amp,
		.drv_name = "sof_sdw",
		.sof_tplg_filename = "sof-mtl-rt715-rt711-rt1308-mono.tplg",
	},
	{},
};
EXPORT_SYMBOL_GPL(snd_soc_acpi_intel_mtl_sdw_machines);
