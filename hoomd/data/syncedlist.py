# Copyright (c) 2009-2021 The Regents of the University of Michigan
# This file is part of the HOOMD-blue project, released under the BSD 3-Clause
# License.

"""Synced list utility classes."""

from collections.abc import MutableSequence
import inspect
from copy import copy

from ..util import _islice_index


class _PartialIsInstance:
    """Allows partial function application of isinstance over classes.

    This is a solution to avoid lambdas to enable pickling. We cannot use
    functools.partial since we need to partially apply the second argument.
    """

    def __init__(self, classes):
        self.classes = classes

    def __call__(self, instance):
        return isinstance(instance, self.classes)


class _PartialGetAttr:
    """Allows partial function application of isinstance over attributes.

    This is a solution to avoid lambdas to enable pickling. We cannot use
    functools.partial since we need to partially apply the second argument.
    """

    def __init__(self, attr):
        self.attr = attr

    def __call__(self, obj):
        return getattr(obj, self.attr)


def identity(obj):
    """Returns obj."""
    return obj


class SyncedList(MutableSequence):
    """Provides syncing and validation for a Python and C++ list.

    Warning:
        This class should not be instantiated by users, and this documentation
        is mostly for developers of HOOMD-blue. The class is documentated to
        highlight the object's API which is that of a `MutableSequence`.

    This class ensures that standard list methods affect both
    Python and C++.

    Args:
        validation (callable or class): A callable that takes one argument
            and returns a boolean based on whether the value is appropriate for
            the list. Can raise ValueError for helpful diagnosis of the problem
            with validation. Alternatively validation can be a class which
            indicates the expected type of items of the list.
        to_synced_list (callable, optional): A callable that takes one
            argument (a Python SyncedList) and does necessary
            conversion before adding to the C++ list. Defaults to simply passing
            the object to C++.
        iterable (iterable, optional): An iterable whose members are valid
            members of the SyncedList instance. Defaults to None which causes
            SyncedList to start with an empty list.
        callable_class (bool, optional): If a class is passed as validation and
            this is ``True`` (defaults to ``False``), then the class will be
            treated as a callable and not used for type checking.
        attach_members (bool, optional): Whether list members can be attached
            (defaults to ``True``). If ``True`` then the `SyncedList` object
            handles adding, attaching, detaching, and removing. If not, these
            steps are skipped regardless of synced status.
    """

    # Also guarantees that lists remain in same order when using the public API.

    def __init__(self,
                 validation,
                 to_synced_list=None,
                 iterable=None,
                 callable_class=False,
                 attach_members=True):
        self._attach_members = attach_members
        if to_synced_list is None:
            to_synced_list = identity

        if inspect.isclass(validation) and not callable_class:
            self._validate = _PartialIsInstance(validation)
        else:
            self._validate = validation

        self._to_synced_list_conversion = to_synced_list
        self._simulation = None
        self._list = []
        if iterable is not None:
            for it in iterable:
                self.append(it)

    def __len__(self):
        """int: Length of the list."""
        return len(self._list)

    def __setitem__(self, index, value):
        """Set self[index] to value.

        Detaches removed value and syncs cpp_list if necessary.
        """
        # Convert negative to positive indices and validate index
        index = self._handle_int(index)
        value = self._validate_or_error(value)
        self._attach_value(value)
        # If synced need to change cpp_list and detach operation before
        # changing python list
        if self._synced:
            self._synced_list[index] = \
                self._to_synced_list_conversion(value)
        self._detach_value(self._list[index])
        self._list[index] = value

    def __getitem__(self, index):
        """Grabs the python list item."""
        index = self._handle_index(index)
        if inspect.isgenerator(index):
            return [self._list[i] for i in index]
        return self._list[index]

    def __delitem__(self, index):
        """Deletes an item from list. Handles detaching if necessary."""
        index = self._handle_index(index)
        if inspect.isgenerator(index):
            # We must iterate from highest value to lowest to ensure we don't
            # accidentally try to delete an index that doesn't exist any more.
            for i in sorted(index, reverse=True):
                del self[i]
            return
        # Since delitem may not del the underlying object, we need to
        # manually call detach here.
        if self._synced:
            del self._synced_list[index]
        old_value = self._list.pop(index)
        self._detach_value(old_value)

    def insert(self, index, value):
        """Insert value to list at index, handling list syncing."""
        value = self._validate_or_error(value)
        self._attach_value(value)
        # Wrap index like normal but allow for inserting a new element to the
        # end of the list.
        if index < len(self):
            index = self._handle_int(index)
        if self._synced:
            self._synced_list.insert(index,
                                     self._to_synced_list_conversion(value))
        self._list.insert(index, value)

    @property
    def _synced(self):
        """Has a cpp_list object means that we are currently syncing."""
        return hasattr(self, "_synced_list")

    def _handle_int(self, integer):
        """Converts negative indices to positive and validates index."""
        if integer < 0:
            if -integer > len(self):
                raise IndexError(
                    f"Negative index {integer} is too small for list of length "
                    f"{len(self)}")
            return integer % max(1, len(self))
        if integer >= len(self):
            raise IndexError(
                f"Index {integer} is outside bounds of a length {len(self)}"
                f"list.")
        return integer

    def _handle_index(self, index):
        if not isinstance(index, slice):
            return self._handle_int(index)
        return self._handle_slice(index)

    def _handle_slice(self, index):
        yield from (self._handle_int(i) for i in _islice_index(self, index))

    def synced_iter(self):
        """Iterate over values in the list. Does nothing when not synced."""
        if self._synced:
            yield from self._synced_list

    def _attach_value(self, value, raise_if_added=True):
        """Attaches and/or adds value to simulation if unattached.

        Raises an error if value is already in this or another list.
        """
        if not self._attach_members:
            return
        if raise_if_added and value._added:
            raise RuntimeError(f"Object {value} cannot be added to two lists.")
        value._add(self._simulation)
        if self._synced:
            value._attach()

    def _detach_value(self, value):
        """Detaches and/or removes value to simulation if attached."""
        if not self._attach_members:
            return
        if self._synced:
            value._detach()
        if value._added:
            value._remove()

    def _validate_or_error(self, value):
        """Complete error checking and processing of value."""
        try:
            if self._validate(value):
                return value
            else:
                raise ValueError(f"Value {value} could not be validated.")
        except ValueError as verr:
            raise ValueError(f"Validation failed: {verr.args[0]}")

    def _sync(self, simulation, synced_list):
        """Attach all list items and update for automatic attachment."""
        self._simulation = simulation
        self._synced_list = synced_list
        for item in self:
            self._attach_value(item, False)
            self._synced_list.append(self._to_synced_list_conversion(item))

    def _unsync(self):
        """Detach all items, clear _synced_list, and remove cpp references."""
        if not self._synced:
            return
        # while not strictly necessary we check for self._attach_members
        # here to avoid looping if necessary.
        if self._attach_members:
            for item in self:
                self._detach_value(item)
        del self._simulation
        del self._synced_list

    def __getstate__(self):
        """Get state for pickling."""
        state = copy(self.__dict__)
        state['_simulation'] = None
        state.pop('_synced_list', None)
        return state

    def __eq__(self, other):
        """Test for equality."""
        return (len(self) == len(other)
                and all(a == b for a, b in zip(self, other)))
