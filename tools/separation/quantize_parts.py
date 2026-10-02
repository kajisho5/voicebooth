import onnx, os, sys
from onnx import numpy_helper, helper
from onnxruntime.quantization import quantize_dynamic, QuantType
os.makedirs('parts8', exist_ok=True)
for name in sorted(os.listdir('parts')):
    m = onnx.load('parts/' + name)
    inits = {i.name: i for i in m.graph.initializer}
    new_nodes = []
    for n in m.graph.node:
        if n.op_type == 'Gemm' and n.input[1] in inits:
            A, W = n.input[0], n.input[1]
            transB = next((a.i for a in n.attribute if a.name == 'transB'), 0)
            w = numpy_helper.to_array(inits[W]); wt = (w.T if transB else w).copy()
            nm = W + '_T'
            m.graph.initializer.remove(inits[W]); m.graph.initializer.append(numpy_helper.from_array(wt, nm))
            if len(n.input) > 2:
                mid = n.output[0] + '_mm'
                new_nodes.append(helper.make_node('MatMul', [A, nm], [mid], name=n.name + '_mm'))
                new_nodes.append(helper.make_node('Add', [mid, n.input[2]], list(n.output), name=n.name + '_add'))
            else:
                new_nodes.append(helper.make_node('MatMul', [A, nm], list(n.output), name=n.name + '_mm'))
        else:
            new_nodes.append(n)
    del m.graph.node[:]; m.graph.node.extend(new_nodes)
    onnx.save(m, 'tmp_mm.onnx')
    quantize_dynamic('tmp_mm.onnx', 'parts8/' + name, weight_type=QuantType.QInt8, per_channel=True,
                     op_types_to_quantize=['MatMul'], extra_options={'MatMulConstBOnly': True})
    print(name, os.path.getsize('parts8/' + name) // 2**20, 'MB', flush=True)
os.remove('tmp_mm.onnx')
