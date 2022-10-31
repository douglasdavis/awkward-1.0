# BSD 3-Clause License; see https://github.com/scikit-hep/awkward-1.0/blob/main/LICENSE

import awkward as ak

np = ak.nplikes.NumpyMetadata.instance()


@ak._connect.numpy.implements("all")
def all(array, axis=None, keepdims=False, mask_identity=False, flatten_records=False):
    """
    Args:
        array: Array-like data (anything #ak.to_layout recognizes).
        axis (None or int): If None, combine all values from the array into
            a single scalar result; if an int, group by that axis: `0` is the
            outermost, `1` is the first level of nested lists, etc., and
            negative `axis` counts from the innermost: `-1` is the innermost,
            `-2` is the next level up, etc.
        keepdims (bool): If False, this reducer decreases the number of
            dimensions by 1; if True, the reduced values are wrapped in a new
            length-1 dimension so that the result of this operation may be
            broadcasted with the original array.
        mask_identity (bool): If True, reducing over empty lists results in
            None (an option type); otherwise, reducing over empty lists
            results in the operation's identity.
        flatten_records (bool): If True, axis=None combines fields from different
            records; otherwise, records raise an error.

    Returns True in each group of elements from `array` (many types supported,
    including all Awkward Arrays and Records) if all values are True; False
    otherwise. Thus, it represents reduction over the "logical and" operation,
    whose identity is True (i.e. asking if all the values are True in an
    empty list results in True). This operation is the same as NumPy's
    [all](https://docs.scipy.org/doc/numpy/reference/generated/numpy.all.html)
    if all lists at a given dimension have the same length and no None values,
    but it generalizes to cases where they do not.

    See #ak.sum for a more complete description of nested list and missing
    value (None) handling in reducers.
    """
    with ak._errors.OperationErrorContext(
        "ak.all",
        dict(
            array=array,
            axis=axis,
            keepdims=keepdims,
            mask_identity=mask_identity,
            flatten_records=flatten_records,
        ),
    ):
        return _impl(array, axis, keepdims, mask_identity, flatten_records)


def _impl(array, axis, keepdims, mask_identity, flatten_records):
    layout = ak.operations.to_layout(array, allow_record=False, allow_other=False)

    if axis is None:
        if not layout.nplike.known_data or not layout.nplike.known_shape:
            reducer_cls = ak._reducers.All

            def map(x):
                return ak._typetracer.UnknownScalar(
                    np.dtype(reducer_cls.return_dtype(x.dtype))
                )

        else:

            def map(x):
                return layout.nplike.all(x.data)

        def reduce(xs):
            if len(xs) == 1:
                return xs[0]
            else:
                return layout.nplike.logical_and(xs[0], reduce(xs[1:]))

        return reduce(
            [
                map(x)
                for x in layout.completely_flatten(
                    function_name="ak.all", flatten_records=flatten_records
                )
            ]
        )

    else:
        behavior = ak._util.behavior_of(array)
        out = layout.all(
            axis=axis, mask=mask_identity, keepdims=keepdims, behavior=behavior
        )
        if isinstance(out, (ak.contents.Content, ak.record.Record)):
            return ak._util.wrap(out, behavior)
        else:
            return out