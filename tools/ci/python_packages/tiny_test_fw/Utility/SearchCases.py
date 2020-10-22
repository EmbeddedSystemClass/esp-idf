# Copyright 2015-2017 Espressif Systems (Shanghai) PTE LTD
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http:#www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

""" search test cases from a given file or path """
import os
import fnmatch
import types
import copy

from . import load_source


class Search(object):
    TEST_CASE_FILE_PATTERN = "*_test.py"
    SUPPORT_REPLICATE_CASES_KEY = ['target']

    @classmethod
    def _search_cases_from_file(cls, file_name):
        """ get test cases from test case .py file """

        print("Try to get cases from: " + file_name)
        test_functions = []
        try:
            mod = load_source(file_name)
            for func in [mod.__getattribute__(x) for x in dir(mod)
                         if isinstance(mod.__getattribute__(x), types.FunctionType)]:
                try:
                    # test method decorator will add test_method attribute to test function
                    if func.test_method:
                        test_functions.append(func)
                except AttributeError:
                    continue
        except ImportError as e:
            print("ImportError: \r\n\tFile:" + file_name + "\r\n\tError:" + str(e))

        test_functions_out = []
        for case in test_functions:
            test_functions_out += cls.replicate_case(case)

        for i, test_function in enumerate(test_functions_out):
            print("\t{}. ".format(i + 1) + test_function.case_info["name"])
            test_function.case_info['app_dir'] = os.path.dirname(file_name)
        return test_functions_out

    @classmethod
    def _search_test_case_files(cls, test_case, file_pattern):
        """ search all test case files recursively of a path """

        if not os.path.exists(test_case):
            raise OSError("test case path not exist")
        if os.path.isdir(test_case):
            test_case_files = []
            for root, _, file_names in os.walk(test_case):
                for filename in fnmatch.filter(file_names, file_pattern):
                    test_case_files.append(os.path.join(root, filename))
        else:
            test_case_files = [test_case]
        return test_case_files

    @classmethod
    def replicate_case(cls, case):
        """
        Replicate cases according to its filter values.
        If one case has specified filter chip=(ESP32, ESP32C),
        it will create 2 cases, one for ESP32 and on for ESP32C.
        Once the cases are replicated, it's easy to filter those we want to execute.

        :param case: the original case
        :return: a list of replicated cases
        """
        replicate_config = []
        for key in case.case_info:
            if key == 'ci_target':  # ci_target is used to filter target, should not be duplicated.
                continue
            if isinstance(case.case_info[key], (list, tuple)):
                replicate_config.append(key)

        def _replicate_for_key(cases, replicate_key, replicate_list):
            def deepcopy_func(f, name=None):
                fn = types.FunctionType(f.__code__, f.__globals__, name if name else f.__name__,
                                        f.__defaults__, f.__closure__)
                fn.__dict__.update(copy.deepcopy(f.__dict__))
                return fn

            case_out = []
            for inner_case in cases:
                for value in replicate_list:
                    new_case = deepcopy_func(inner_case)
                    new_case.case_info[replicate_key] = value
                    case_out.append(new_case)
            return case_out

        replicated_cases = [case]
        while replicate_config:
            if not replicate_config:
                break
            key = replicate_config.pop()
            if key in cls.SUPPORT_REPLICATE_CASES_KEY:
                replicated_cases = _replicate_for_key(replicated_cases, key, case.case_info[key])

        # mark the cases with targets not in ci_target
        for case in replicated_cases:
            ci_target = case.case_info['ci_target']
            if not ci_target or case.case_info['target'] in ci_target:
                case.case_info['supported_in_ci'] = True
            else:
                case.case_info['supported_in_ci'] = False

        return replicated_cases

    @classmethod
    def search_test_cases(cls, test_case_paths, test_case_file_pattern=None):
        """
        search all test cases from a folder or file, and then do case replicate.

        :param test_case_paths: test case file(s) paths
        :param test_case_file_pattern: unix filename pattern
        :return: a list of replicated test methods
        """
        if not isinstance(test_case_paths, list):
            test_case_paths = [test_case_paths]
        test_case_files = []
        for path in test_case_paths:
            test_case_files.extend(cls._search_test_case_files(path, test_case_file_pattern or cls.TEST_CASE_FILE_PATTERN))
        test_cases = []
        for test_case_file in test_case_files:
            test_cases += cls._search_cases_from_file(test_case_file)
        return test_cases
