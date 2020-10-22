# -*- coding: utf-8 -*-
#
# English Language RTD & Sphinx config file
#
# Uses ../conf_common.py for most non-language-specific settings.
# Importing conf_common adds all the non-language-specific
# parts to this conf module
try:
    from conf_common import *  # noqa: F403,F401
except ImportError:
    import sys
    import os
    sys.path.insert(0, os.path.abspath('../..'))
    from conf_common import *  # noqa: F403,F401

# General information about the project.
project = u'ESP-IDF Programming Guide'
copyright = u'2016 - 2020, Espressif Systems (Shanghai) CO., LTD'

# The language for content autogenerated by Sphinx. Refer to documentation
# for a list of supported languages.
language = 'en'
html_copy_source = False


html_logo = None
latex_logo = None
html_static_path = []

conditional_include_dict = {'esp32':["esp32_page.rst"],
                            'esp32s2':["esp32s2_page.rst"],
                            'SOC_BT_SUPPORTED':["bt_page.rst"],
                            }
