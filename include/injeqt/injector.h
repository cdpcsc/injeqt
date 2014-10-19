/*
 * %injeqt copyright begin%
 * Copyright 2014 Rafał Malinowski (rafal.przemyslaw.malinowski@gmail.com)
 * %injeqt copyright end%
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#pragma once

#include <injeqt/injeqt.h>
#include <injeqt/type.h>

#include <memory>
#include <vector>
#include <QtCore/QObject>

/**
 * @file
 * @brief Contains classes and functions for creating injectors.
 */

namespace injeqt { namespace internal {
	class injector_impl;
}}

namespace injeqt { namespace v1 {

class module;

/**
 * @brief Injector is created from set of modules that contains injectable types.
 *
 * Injector is constructed from set of modules that contains groups of related types.
 * Then it can be used to get instances of configured types that have their dependencies
 * resolved and set - there is no need to manually wire services and factories.
 *
 * If two or more modules have the same type configured an exception is thrown during construction.
 * Otherwise, from all types in modules a set of available types is computed. Each type directly
 * added to modules is added to it. Then all unique super types are added to this set. This means
 * that two types with common ancestor can be used in injection. Note that Qt meta object system
 * only supports single inheritance.
 *
 * Injector takes ownership of modules passed to it. Also all objects created inside injector
 * (using default constructor - configured with module::add_type<T>(), or factory - configured
 * with module::add_factory<T, F>()) are under its ownership. Lifetime of objects added as ready
 * objects (configured with module::add_ready_object<T>(QObject *) is not managed by injector.
 * For clarity ready objects can be stored in module instances as unique pointers. Injector will own
 * then as it own modules.
 */
class INJEQT_API injector final
{

public:
	/**
	 * @brief Create empty injector with no types configured.
	 *
	 * Empty injector will not be able to produce any object of any type.
	 */
	injector();

	/**
	 * @brief Create new injector from provided modules.
	 * @param modules list of modules
	 * @throw ambiguous_types if one or more types in @p modules is ambiguous
	 * @throw unresolvable_dependencies if a type with unresolvable dependency is found in @p modules
	 * @throw dependency_duplicated when one type occurs twice as a dependency
	 * @throw dependency_on_self when type depends on self
	 * @throw dependency_on_subtype when type depends on own supertype
	 * @throw dependency_on_subtype when type depends on own subtype
	 * @throw invalid_setter if any tagged setter has parameter that is not a QObject-derived pointer
	 * @throw invalid_setter if any tagged setter has parameter that is a QObject pointer
	 * @throw invalid_setter if any tagged setter has other number of parameters than one
	 *
	 * Creates injector with all types from modules configured. If combined configuration
	 * of all modules is invalid an exception is thrown. Configuration is invalid when:
	 * * any type is configured in more than one module
	 * * a dependency exists with type that is non configured in any module
	 * * a cycle of factories is found (currently does not throw an exception)
	 */
	explicit injector(std::vector<std::unique_ptr<module>> modules);
	injector(injector &&x);
	~injector();

	injector & operator = (injector &&x);

	/**
	 * @brief Returns pointer to object of given type T.
	 * @tparam T type of object to return
	 * @throw empty_type if T is not valid QObject derived type
	 * @throw qobject_type if T is QObject
	 * @throw unknown_type if @p interface_type was not configured in injector
	 * @throw instantiation_failed if instantiation of one of required types failed
	 *
	 * When object of given type is requested by get<T>() method, injector first check if T is in set of
	 * available types. If not, an exception is thrown. Next an unique configured type U that implements T
	 * is found. If object of type U was already created, it is returned. If not, it is created with all
	 * dependencies it requires. Depending on configuration of U it can be created directly by U default
	 * constructor or it can be created by factory that is assigned to that type (note: injector will
	 * create itself all required factories with the same alghoritm). After U with all its dependencies
	 * is created all dependency setters are called with proper arguments. Then U object is added to cache
	 * and is itself returned.
	 */
	template<typename T>
	T * get()
	{
		return qobject_cast<T *>(get(make_type<T>()));
	}

	/**
	 * @brief Returns pointer to object of given type interface_type.
	 * @param interface_type type of object to return
	 * @throw empty_type if interface_type is empty
	 * @throw qobject_type if interface_type represents QObject
	 * @throw unknown_type if @p interface_type was not configured in injector
	 * @throw instantiation_failed if instantiation of one of required types failed
	 *
	 * @see T * get<T>()
	 */
	QObject * get(const type &interface_type);

private:
	std::unique_ptr<injeqt::internal::injector_impl> _pimpl;

};

}}