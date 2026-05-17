from tools.hlsflow.action_key import ActionKeySpec


def _host(mode, *, target_kind="accelerator", target="u250", sysroot=None):
    toolchain = {"compiler": "g++", "xrt_abi": "2024.2", "profile": "release"}
    if sysroot:
        toolchain["sysroot"] = sysroot
    return ActionKeySpec(
        stage="host",
        target_identity=target,
        mode_identity=mode,
        target_kind=target_kind,
        toolchain=toolchain,
        declared_inputs=[{"path": "src/host/main.cpp", "sha256": "a"}],
        output_names=["run_saxpy"],
    ).key()


def test_accelerator_host_key_is_shared_across_modes_when_abi_inputs_match():
    assert _host("sw_emu") == _host("hw_emu") == _host("hw")


def test_embedded_host_key_depends_on_target_and_sysroot():
    zcu102 = _host("hw", target_kind="embedded", target="zcu102", sysroot="/sdk/a")
    zcu104 = _host("hw", target_kind="embedded", target="zcu104", sysroot="/sdk/a")
    sysroot_b = _host("hw", target_kind="embedded", target="zcu102", sysroot="/sdk/b")
    assert zcu102 != zcu104
    assert zcu102 != sysroot_b


def _emconfig(mode):
    return ActionKeySpec(
        stage="emconfig",
        target_identity="u250",
        mode_identity=mode,
        platform={"path": "/x/u250.xpfm", "device_count": 1},
        command_args=["emconfigutil", "--platform", "/x/u250.xpfm"],
        output_names=["emconfig.json"],
    ).key()


def test_emconfig_key_is_shared_across_sw_and_hw_emu():
    assert _emconfig("sw_emu") == _emconfig("hw_emu")


def _xclbin(mode):
    return ActionKeySpec(
        stage="xclbin",
        target_identity="u250",
        mode_identity=mode,
        platform={"path": "/x/u250.xpfm"},
        declared_inputs=[{"path": "saxpy.xo", "sha256": "xo"}],
        command_args=["v++", "--link", "--target", mode],
        output_names=["saxpy.xclbin"],
    ).key()


def test_xclbin_key_is_mode_specific():
    assert len({_xclbin("sw_emu"), _xclbin("hw_emu"), _xclbin("hw")}) == 3


def _csynth(mode, *, mode_affects_output=False, extra_arg=None):
    args = ["v++", "--compile", "--mode", "hls"]
    if extra_arg:
        args.append(extra_arg)
    return ActionKeySpec(
        stage="csynth",
        target_identity="u250",
        mode_identity=mode,
        platform={"path": "/x/u250.xpfm", "part": "xcu250", "clock": "300"},
        declared_inputs=[{"path": "saxpy.cpp", "sha256": "src"}],
        command_args=args,
        output_names=["saxpy.xo"],
        mode_affects_output=mode_affects_output,
    ).key()


def test_csynth_ignores_mode_unless_mode_affects_output():
    assert _csynth("hw") == _csynth("hw_emu")
    assert _csynth("hw", mode_affects_output=True) != _csynth("hw_emu", mode_affects_output=True)
    assert _csynth("hw", extra_arg="--foo=hw") != _csynth("hw_emu", extra_arg="--foo=hw_emu")


def _cosim(mode, *, mode_affects_output=False):
    return ActionKeySpec(
        stage="cosim",
        target_identity="u250",
        mode_identity=mode,
        platform={"path": "/x/u250.xpfm"},
        declared_inputs=[
            {"path": "saxpy.xo", "sha256": "xo"},
            {"path": "saxpy_tb.cpp", "sha256": "tb"},
        ],
        command_args=["v++", "--compile", "--mode", "hls", "--cosim"],
        output_names=[".cosim.stamp"],
        mode_affects_output=mode_affects_output,
    ).key()


def test_cosim_ignores_mode_unless_simulator_config_differs():
    assert _cosim("hw") == _cosim("hw_emu")
    assert _cosim("hw", mode_affects_output=True) != _cosim("hw_emu", mode_affects_output=True)


def test_hls_model_key_omits_vitis_xrt_platform_leakage():
    base = ActionKeySpec(
        stage="hls-model",
        target_identity="u250",
        mode_identity="hw",
        toolchain={"vitis": "2024.2", "xrt": "2024.2"},
        platform={"path": "/x/u250.xpfm"},
        declared_inputs=[{"path": "model.cpp", "sha256": "m"}],
        output_names=["hls_model_out.bin"],
    )
    changed_tool = ActionKeySpec(
        stage="hls-model",
        target_identity="u55c",
        mode_identity="hw_emu",
        toolchain={"vitis": "2025.1", "xrt": "2025.1"},
        platform={"path": "/x/u55c.xpfm"},
        declared_inputs=[{"path": "model.cpp", "sha256": "m"}],
        output_names=["hls_model_out.bin"],
    )
    assert base.key() == changed_tool.key()
