def test_display_lovyangfx_manual_start(arduino_cli_upload):
    # pytest-embedded-arduino-cli builds and uploads from sketch.yaml before
    # this test body runs. For the display board, upload starts the foreground
    # SDL2 app and returns after the user closes the window.
    assert arduino_cli_upload is None
