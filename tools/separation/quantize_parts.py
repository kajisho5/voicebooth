"""分割 ONNX（fp32）を int8 にする：重みだけ per-channel・動的量子化（MatMul の定数側だけ）。Gemm は先に MatMul + Add に直す。
Mel-Band RoFormer（Kim）も BS-RoFormer（anvuew）も同じ手順。

使い方：
  python quantize_parts.py                      parts/ -> parts8/（Kim の既定。前と同じ）
  python quantize_parts.py <in_dir> <out_dir>   例：bs_ft1_parts bs-roformer-anvuew-ft1-int8-1
in_dir に parts.json があれば out_dir に写す。
"""
import onnx, os, shutil, sys
from onnx import numpy_helper, helper
from onnxruntime.quantization import quantize_dynamic, QuantType

src = sys.argv[1] if len(sys.argv) > 1 else 'parts'
dst = sys.argv[2] if len(sys.argv) > 2 else 'parts8'
os.makedirs(dst, exist_ok=True)
tmp = os.path.join(dst, 'tmp_mm.onnx')
for name in sorted(os.listdir(src)):
    if not name.endswith('.onnx'):
        continue
    m = onnx.load(os.path.join(src, name))
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
    onnx.save(m, tmp)
    quantize_dynamic(tmp, os.path.join(dst, name), weight_type=QuantType.QInt8, per_channel=True,
                     op_types_to_quantize=['MatMul'], extra_options={'MatMulConstBOnly': True})
    print(name, os.path.getsize(os.path.join(dst, name)) // 2**20, 'MB', flush=True)
if os.path.exists(tmp):
    os.remove(tmp)
if os.path.exists(os.path.join(src, 'parts.json')):
    shutil.copyfile(os.path.join(src, 'parts.json'), os.path.join(dst, 'parts.json'))
