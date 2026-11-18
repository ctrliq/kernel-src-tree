// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2026 Google LLC */
#include <linux/bpf.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <test_progs.h>

static void test_map_info_tail_zero(void)
{
	LIBBPF_OPTS(bpf_map_create_opts, map_opts);
	struct bpf_map_info_fake {
		__u8 info[offsetofend(struct bpf_map_info, hash_size)];
		__u32 pad;
	} info = {
		.pad = 1,
	};
	int map_fd, err;
	__u32 info_len;

	map_fd = bpf_map_create(BPF_MAP_TYPE_ARRAY, "arr", sizeof(int), 1, 1, &map_opts);
	if (!ASSERT_GE(map_fd, 0, "bpf_map_create"))
		return;

	info_len = sizeof(info);
	err = bpf_obj_get_info_by_fd(map_fd, &info, &info_len);
	ASSERT_EQ(err, -E2BIG, "bpf_obj_get_info_by_fd");

	close(map_fd);
}

static void test_prog_info_tail_zero(void)
{
	LIBBPF_OPTS(bpf_prog_load_opts, prog_opts);
	struct bpf_insn insns[] = {
		BPF_MOV64_IMM(BPF_REG_0, 0),
		BPF_EXIT_INSN(),
	};
	struct bpf_prog_info_fake {
		__u8 info[offsetofend(struct bpf_prog_info, attach_btf_id)];
		__u32 pad;
	} info = {
		.pad = 1,
	};
	int prog_fd, err;
	__u32 info_len;

	prog_fd = bpf_prog_load(BPF_PROG_TYPE_SOCKET_FILTER, "test_prog", "GPL", insns,
				ARRAY_SIZE(insns), &prog_opts);
	if (!ASSERT_GE(prog_fd, 0, "bpf_prog_load"))
		return;

	info_len = sizeof(info);
	err = bpf_obj_get_info_by_fd(prog_fd, &info, &info_len);
	ASSERT_EQ(err, -E2BIG, "bpf_obj_get_info_by_fd");

	close(prog_fd);
}

void test_bpf_attr_size(void)
{
	if (test__start_subtest("map_info_tail_zero"))
		test_map_info_tail_zero();
	if (test__start_subtest("prog_info_tail_zero"))
		test_prog_info_tail_zero();
}
