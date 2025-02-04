// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "absl/flags/flag.h"
#include "absl/strings/string_view.h"
#include "benchmark/benchmark.h"
#include "iree/base/file_io.h"
#include "iree/base/status.h"
#include "iree/base/tracing.h"
#include "iree/modules/hal/hal_module.h"
#include "iree/tools/vm_util.h"
#include "iree/vm/api.h"
#include "iree/vm/bytecode_module.h"

// TODO(gcmn): Allow stdin in a non-gross way. The benchmark framework invokes
// the benchmarking function multiple times, so we have to do something to only
// process stdin once. Probably requires dynamic benchmark registration.
ABSL_FLAG(std::string, input_file, "",
          "File containing the module to load that contains the entry "
          "function. Required and cannot be stdin.");

ABSL_FLAG(std::string, entry_function, "",
          "Name of a function contained in the module specified by input_file "
          "to run.");

ABSL_FLAG(std::string, driver, "vmla", "Backend driver to use.");

ABSL_FLAG(std::vector<std::string>, inputs, {},
          "A comma-separated list of of input buffers of the format:"
          "[shape]xtype=[value]\n"
          "2x2xi32=1 2 3 4\n"
          "Optionally, brackets may be used to separate the element values. "
          "They are ignored by the parser.\n"
          "2x2xi32=[[1 2][3 4]]\n"
          "Due to the absence of repeated flags in absl, commas should not be "
          "used to separate elements. They are reserved for separating input "
          "values:\n"
          "2x2xi32=[[1 2][3 4]], 1x2xf32=[[1 2]]");

ABSL_FLAG(std::string, inputs_file, "",
          "Provides a file for input shapes and optional values (see "
          "ParseToVariantListFromFile in vm_util.h for details)");

namespace iree {
namespace {

StatusOr<std::string> GetModuleContentsFromFlags() {
  IREE_TRACE_SCOPE0("GetModuleContentsFromFlags");
  auto input_file = absl::GetFlag(FLAGS_input_file);
  if (input_file.empty()) {
    return InvalidArgumentErrorBuilder(IREE_LOC)
           << "input_file must be specified";
  }
  return file_io::GetFileContents(input_file);
}

Status Run(::benchmark::State& state) {
  IREE_TRACE_SCOPE0("iree-benchmark-module");

  IREE_RETURN_IF_ERROR(iree_hal_module_register_types())
      << "registering HAL types";
  iree_vm_instance_t* instance = nullptr;
  IREE_RETURN_IF_ERROR(
      iree_vm_instance_create(iree_allocator_system(), &instance))
      << "creating instance";

  IREE_ASSIGN_OR_RETURN(auto module_data, GetModuleContentsFromFlags());
  iree_vm_module_t* input_module = nullptr;
  IREE_RETURN_IF_ERROR(LoadBytecodeModule(module_data, &input_module));

  iree_hal_device_t* device = nullptr;
  IREE_RETURN_IF_ERROR(CreateDevice(absl::GetFlag(FLAGS_driver), &device));
  iree_vm_module_t* hal_module = nullptr;
  IREE_RETURN_IF_ERROR(CreateHalModule(device, &hal_module));

  iree_vm_context_t* context = nullptr;
  // Order matters. The input module will likely be dependent on the hal module.
  std::array<iree_vm_module_t*, 2> modules = {hal_module, input_module};
  IREE_RETURN_IF_ERROR(iree_vm_context_create_with_modules(
      instance, modules.data(), modules.size(), iree_allocator_system(),
      &context))
      << "creating context";

  std::string function_name = absl::GetFlag(FLAGS_entry_function);
  iree_vm_function_t function;
  IREE_RETURN_IF_ERROR(input_module->lookup_function(
      input_module->self, IREE_VM_FUNCTION_LINKAGE_EXPORT,
      iree_string_view_t{function_name.data(), function_name.size()},
      &function))
      << "looking up function '" << function_name << "'";

  IREE_RETURN_IF_ERROR(ValidateFunctionAbi(function));
  IREE_ASSIGN_OR_RETURN(auto input_descs, ParseInputSignature(function));

  vm::ref<iree_vm_list_t> inputs;
  if (!absl::GetFlag(FLAGS_inputs_file).empty()) {
    if (!absl::GetFlag(FLAGS_inputs).empty()) {
      return InvalidArgumentErrorBuilder(IREE_LOC)
             << "Expected only one of inputs and inputs_file to be set";
    }
    IREE_ASSIGN_OR_RETURN(
        inputs, ParseToVariantListFromFile(input_descs,
                                           iree_hal_device_allocator(device),
                                           absl::GetFlag(FLAGS_inputs_file)));
  } else {
    IREE_ASSIGN_OR_RETURN(
        inputs,
        ParseToVariantList(input_descs, iree_hal_device_allocator(device),
                           absl::GetFlag(FLAGS_inputs)));
  }

  IREE_ASSIGN_OR_RETURN(auto output_descs, ParseOutputSignature(function));

  // Execute once to make sure any first-iteration outliers are eliminated (e.g.
  // JITing the SPIR-V) and clearly separate out benchmark-related problems in
  // future debugging.
  {
    vm::ref<iree_vm_list_t> outputs;
    IREE_RETURN_IF_ERROR(
        iree_vm_list_create(/*element_type=*/nullptr, output_descs.size(),
                            iree_allocator_system(), &outputs));
    IREE_RETURN_IF_ERROR(iree_vm_invoke(context, function, /*policy=*/nullptr,
                                        inputs.get(), outputs.get(),
                                        iree_allocator_system()));
  }

  for (auto _ : state) {
    // No status conversions and conditional returns in the benchmarked inner
    // loop.
    vm::ref<iree_vm_list_t> outputs;
    IREE_CHECK_OK(iree_vm_list_create(/*element_type=*/nullptr,
                                      output_descs.size(),
                                      iree_allocator_system(), &outputs));
    IREE_CHECK_OK(iree_vm_invoke(context, function, /*policy=*/nullptr,
                                 inputs.get(), outputs.get(),
                                 iree_allocator_system()));
  }

  iree_vm_module_release(hal_module);
  iree_vm_module_release(input_module);
  iree_hal_device_release(device);
  iree_vm_context_release(context);
  iree_vm_instance_release(instance);
  return OkStatus();
}

void BM_RunModule(benchmark::State& state) {
  // Delegate to a status-returning function so we can use the status macros.
  IREE_CHECK_OK(Run(state));
}

BENCHMARK(BM_RunModule)
    // By default only the main thread is included in CPU time. Include all the
    // threads instead.
    ->MeasureProcessCPUTime()
    // To make single and multi-threaded benchmarks more comparable, use the
    // wall time to determine how many iterations to run.
    // See https://github.com/google/benchmark#cpu-timers,
    ->UseRealTime()
    // Report timing in milliseconds, which is the general order of magnitude of
    // model runs. The benchmark framework will print with precision between 0
    // and 3 places after the decimal while aiming for three significant digits.
    // If we end up wanting precision beyond microseconds, we can make this
    // setting configurable with a custom command line flag.
    ->Unit(benchmark::kMillisecond);

}  // namespace

}  // namespace iree
