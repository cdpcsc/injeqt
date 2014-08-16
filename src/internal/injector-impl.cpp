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

#include "injector-impl.h"

#include "default-constructor-method.h"
#include "dependencies.h"
#include "provider-by-default-constructor.h"
#include "provider-ready.h"
#include "provider.h"
#include "module-impl.h"
#include "module.h"
#include "required-to-instantiate.h"
#include "resolve-dependencies.h"
#include "resolved-dependency.h"
#include "type-relations.h"

namespace injeqt { namespace internal {
// TODO: tests!!
injector_impl::injector_impl()
{
}

injector_impl::injector_impl(std::vector<std::unique_ptr<module>> modules) :
	_modules{std::move(modules)}
{
	auto all_providers = std::vector<std::unique_ptr<provider>>{};
	for (auto &&module : _modules)
		std::move(std::begin(module->_pimpl->providers()), std::end(module->_pimpl->providers()), std::back_inserter(all_providers));
	_available_providers = providers{std::move(all_providers)};

	auto all_types = std::vector<type>{};
	std::transform(std::begin(_available_providers), std::end(_available_providers), std::back_inserter(all_types),
		[](const std::unique_ptr<provider> &c){ return c->created_type(); });
	auto relations = make_type_relations(all_types);
	_available_types = relations.unique();

	for (auto &&t : all_types)
		if (relations.ambiguous().contains(t))
			throw ambiguous_type_configured{t.name()};

	auto all_dependencies = std::vector<type_dependencies>{};
	std::transform(std::begin(_available_types), std::end(_available_types), std::back_inserter(all_dependencies),
		[](const implemented_by &ib){ return type_dependencies{ib.implementation_type()}; });
	_dependencies = types_dependencies{all_dependencies};
}

QObject * injector_impl::get(const type &interface_type)
{
	auto implementation_type_it = _available_types.get(interface_type);
	if (implementation_type_it == end(_available_types))
		throw type_not_configured_exception{interface_type.name()};

	auto implementation_type = implementation_type_it->implementation_type();
	auto object_it = _objects.get(implementation_type);
	if (object_it != end(_objects))
		return object_it->object();

	// throws if object of implementation_type was not created
	_objects = objects_with(_objects, implementation_type);
	return _objects.get(implementation_type)->object();
}

implementations injector_impl::objects_with(implementations objects, const type &implementation_type)
{
	auto types_to_instantiate = required_to_instantiate(implementation_type, _available_types, _dependencies, objects);
	return objects_with(objects, types_to_instantiate);
}

implementations injector_impl::objects_with(implementations objects, const types &types_to_instantiate)
{
	auto objects_to_resolve = std::vector<implementation>{};
	for (auto &&type_to_instantiate : types_to_instantiate)
	{
		auto provider_it = _available_providers.get(type_to_instantiate);
		if (provider_it == end(_available_providers))
			return objects; // TODO: throw exception

		for (auto &&required_type : provider_it->get()->required_types())
			objects = objects_with(objects, required_type);
	}

	for (auto &&type_to_instantiate : types_to_instantiate)
		if (objects.get(type_to_instantiate) == end(objects))
		{
			auto provider_it = _available_providers.get(type_to_instantiate);
			auto instance = provider_it->get()->create(*this);

			if (instance)
				objects_to_resolve.emplace_back(type_to_instantiate, instance);
			else
				throw type_not_instantiated_exception{type_to_instantiate.name()};
		}

	objects.merge(implementations{objects_to_resolve});

	for (auto &&object_to_resolve : objects_to_resolve)
	{
		auto to_resolve = _dependencies.get(object_to_resolve.interface_type())->dependency_list();
		auto resolved_dependencies = resolve_dependencies(to_resolve, objects);
		if (!resolved_dependencies.unresolved.empty())
			throw unresolved_dependencies_exception{};

		for (auto &&resolved : resolved_dependencies.resolved)
			resolved.apply_on(object_to_resolve.object());
	}

	return objects;
}

}}
