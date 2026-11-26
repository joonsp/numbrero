# Numbrero Omarchy Shell Plugin

Bar widget and quick-calc popup for the Omarchy shell, backed by the native `numbrero` CLI.

## Install

Make sure `numbrero` is installed (`./bin/install` from the repository root), then validate and install the plugin:

```bash
omarchy plugin validate ./omarchy-plugin
cp -r ./omarchy-plugin ~/.config/omarchy/plugins/numbrero.quickcalc
omarchy plugin enable numbrero.quickcalc
```
