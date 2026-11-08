cmake_minimum_required(VERSION 3.20)
# Run only on the clean release staging bundle, never on a developer build.
include(BundleUtilities)
set(BU_CHMOD_BUNDLE_ITEMS ON)
fixup_bundle("${WEB_APP}" "" "")
