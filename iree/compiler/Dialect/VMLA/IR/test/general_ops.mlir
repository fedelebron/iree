// Tests the printing/parsing of the VMLA dialect ops.

// RUN: iree-opt -split-input-file %s | iree-opt -split-input-file | IreeFileCheck %s

// CHECK-LABEL: @unaryOp
// CHECK-SAME: %[[SRC:[a-zA-Z0-9$._-]+]]
// CHECK-SAME: %[[DST:[a-zA-Z0-9$._-]+]]
func @unaryOp(%src : !vmla.buffer, %dst : !vmla.buffer) {
  // CHECK: vmla.log %[[SRC]], out %[[DST]] : f32
  vmla.log %src, out %dst : f32
  return
}

// -----

// CHECK-LABEL: @binaryOp
// CHECK-SAME: %[[LHS:[a-zA-Z0-9$._-]+]]
// CHECK-SAME: %[[RHS:[a-zA-Z0-9$._-]+]]
// CHECK-SAME: %[[DST:[a-zA-Z0-9$._-]+]]
func @binaryOp(%lhs : !vmla.buffer, %rhs : !vmla.buffer, %dst : !vmla.buffer) {
  // CHECK: vmla.atan2 %[[LHS]], %[[RHS]], out %[[DST]] : f32
  vmla.atan2 %lhs, %rhs, out %dst : f32
  return
}

// -----

// CHECK-LABEL: @ternaryOp
// CHECK-SAME: %[[A:[a-zA-Z0-9$._-]+]]
// CHECK-SAME: %[[B:[a-zA-Z0-9$._-]+]]
// CHECK-SAME: %[[C:[a-zA-Z0-9$._-]+]]
// CHECK-SAME: %[[DST:[a-zA-Z0-9$._-]+]]
func @ternaryOp(%a : !vmla.buffer, %b : !vmla.buffer, %c : !vmla.buffer,
                %dst : !vmla.buffer) {
  // CHECK: vmla.clamp %[[A]], %[[B]], %[[C]], out %[[DST]] : f32
  vmla.clamp %a, %b, %c, out %dst : f32
  return
}


// -----

// CHECK-LABEL: @vmla_convert
// CHECK-SAME: %[[SRC:[a-zA-Z0-9$._-]+]]
// CHECK-SAME: %[[DST:[a-zA-Z0-9$._-]+]]
func @vmla_convert(%src : !vmla.buffer, %dst : !vmla.buffer) {
  // CHECK: vmla.convert %[[SRC]], out %[[DST]] : f32 -> i8
  vmla.convert %src, out %dst : f32 -> i8
  return
}

// -----

// CHECK-LABEL: @vmla_batch_matmul_pseudo
// CHECK-SAME: %[[LHS:[a-zA-Z0-9$._-]+]]
// CHECK-SAME: %[[RHS:[a-zA-Z0-9$._-]+]]
func @vmla_batch_matmul_pseudo(%lhs : tensor<32x256x128xf32>,
                               %rhs : tensor<32x1x128xf32>) {
  // CHECK: vmla.batch.matmul.pseudo %[[LHS]], %[[RHS]] :
  // CHECK-SAME: (tensor<32x256x128xf32>, tensor<32x1x128xf32>) ->
  // CHECK-SAME: tensor<32x1x256xf32>
  %dst = vmla.batch.matmul.pseudo %lhs, %rhs :
    (tensor<32x256x128xf32>, tensor<32x1x128xf32>) -> tensor<32x1x256xf32>
  return
}

// -----

// CHECK-LABEL: @vmla_batch_matmul
// CHECK-SAME: %[[LHS:[a-zA-Z0-9$._-]+]]
// CHECK-SAME: %[[LHS_SHAPE:[a-zA-Z0-9$._-]+]]
// CHECK-SAME: %[[RHS:[a-zA-Z0-9$._-]+]]
// CHECK-SAME: %[[RHS_SHAPE:[a-zA-Z0-9$._-]+]]
// CHECK-SAME: %[[DST:[a-zA-Z0-9$._-]+]]
// CHECK-SAME: %[[DST_SHAPE:[a-zA-Z0-9$._-]+]]
func @vmla_batch_matmul(%lhs : !vmla.buffer,
                        %lhs_shape : !shapex.ranked_shape<[8,4,4]>,
                        %rhs : !vmla.buffer,
                        %rhs_shape : !shapex.ranked_shape<[8,1,4]>,
                        %dst : !vmla.buffer,
                        %dst_shape : !shapex.ranked_shape<[8,1,4]>) {
  // CHECK:      vmla.batch.matmul
  // CHECK-SAME: %[[LHS]](%[[LHS_SHAPE]] : !shapex.ranked_shape<[8,4,4]>) : f32,
  // CHECK-SAME: %[[RHS]](%[[RHS_SHAPE]] : !shapex.ranked_shape<[8,1,4]>) : f32,
  // CHECK-SAME: out
  // CHECK-SAME: %[[DST]](%[[DST_SHAPE]] : !shapex.ranked_shape<[8,1,4]>) : f32
  vmla.batch.matmul %lhs(%lhs_shape : !shapex.ranked_shape<[8,4,4]>) : f32,
                    %rhs(%rhs_shape : !shapex.ranked_shape<[8,1,4]>) : f32,
                    out %dst(%dst_shape : !shapex.ranked_shape<[8,1,4]>) : f32
  return
}

// -----

// CHECK-LABEL: @vmla_cmp
// CHECK-SAME: %[[LHS:[a-zA-Z0-9$._-]+]]
// CHECK-SAME: %[[RHS:[a-zA-Z0-9$._-]+]]
// CHECK-SAME: %[[DST:[a-zA-Z0-9$._-]+]]
func @vmla_cmp(%lhs : !vmla.buffer, %rhs : !vmla.buffer, %dst : !vmla.buffer) {
  // CHECK: vmla.cmp "NE", %[[LHS]], %[[RHS]], out %[[DST]] : f16
  vmla.cmp "NE", %lhs, %rhs, out %dst : f16
  return
}

// -----

// CHECK-LABEL: @vmla_select
// CHECK-SAME: %[[COND:[a-zA-Z0-9$._-]+]]
// CHECK-SAME: %[[LHS:[a-zA-Z0-9$._-]+]]
// CHECK-SAME: %[[RHS:[a-zA-Z0-9$._-]+]]
// CHECK-SAME: %[[DST:[a-zA-Z0-9$._-]+]]
func @vmla_select(%cond : !vmla.buffer,
                  %lhs : !vmla.buffer,
                  %rhs : !vmla.buffer,
                  %dst : !vmla.buffer) {
  // CHECK: vmla.select %[[COND]], %[[LHS]], %[[RHS]], out %[[DST]] : f16
  vmla.select %cond, %lhs, %rhs, out %dst : f16
  return
}

// -----

// CHECK-LABEL: @vmla_interface_const
// CHECK-SAME: %[[INTERFACE:[a-zA-Z0-9$._-]+]]
func @vmla_interface_const(%interface : !vmla.interface) {
  // CHECK:      vmla.interface.const %[[INTERFACE]]
  // CHECK-SAME: {offset = 3 : index} : i32
  vmla.interface.const %interface {offset = 3 : index} : i32
  return
}

// -----

// CHECK-LABEL: @vmla_interface_binding
// CHECK-SAME: %[[INTERFACE:[a-zA-Z0-9$._-]+]]
func @vmla_interface_binding(%interface : !vmla.interface) {
  // CHECK:      vmla.interface.binding %[[INTERFACE]]
  // CHECK-SAME: {binding = 0 : i32, set = 0 : i32} : !vmla.buffer
  vmla.interface.binding %interface
                         {binding = 0 : i32, set = 0 : i32} : !vmla.buffer
  return
}
