"""
mcfx_anything parameter-sync test — STUB / deferred.

Why this is a stub
------------------
mcfx_anything is a meta-plugin: it loads another VST/AU and runs N
synchronised instances (one per channel pair). A meaningful sync test must
drive a master parameter, then verify every slave applies the same value.

The blocker for an automated test is *how to make mcfx_anything load an
inner plugin* without user interaction. The plugin choice lives inside the
state blob (see Mcfx_anythingAudioProcessor::setStateInformation in
mcfx_anything/Source/PluginProcessor.cpp:457): the state contains a
<CURRENT_PLUGIN><PLUGIN .../></CURRENT_PLUGIN> child holding a serialised
PluginDescription. Building that blob from Python requires either:

  (a) reimplementing JUCE's custom base64 encoding (alphabet
      ".A-Za-z0-9+", hex-length prefix) and MemoryBlock::copyXmlToBinary's
      8-byte header — roughly 50 lines, doable but uninspected territory; or
  (b) extending mcfx_testhost with `--describe-plugin <path> <out.xml>`
      (KnownPluginList::scanAndAddFile + PluginDescription::createXml) and
      `--make-anything-state <inner-xml> <out.bin>` helpers, so the test
      can stay in Python.

Neither path is hard, just out of scope for the current pass. Both
mechanisms are listed under "Phase 3" of the plan at
~/.claude/plans/audit-the-test-suite-swift-koala.md.

What we want this test to assert
--------------------------------
1. Load mcfx_anything with mcfx_filter as inner plugin (via state blob).
2. Set the master plugin's "Peak 1 Gain" to a known non-default value (e.g.
   +9 dB at 1 kHz).
3. Process white noise through 2N channels.
4. Compute each channel's frequency response. Assert the magnitude bump at
   1 kHz is identical across every channel pair (sync evidence).

The existing test_smoke_loads.py already verifies mcfx_anything opens and
runs to finite output; this test is the deeper sync regression.
"""

import pytest


@pytest.mark.skip(reason=(
    "mcfx_anything sync test requires either a JUCE-base64 helper in Python "
    "or a --describe-plugin / --make-anything-state flag on mcfx_testhost. "
    "See module docstring."
))
def test_anything_master_param_syncs_to_slaves():
    raise NotImplementedError
