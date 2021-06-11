/*

MIT License

Copyright (c) 2021 Marc "Foddex" Oude Kotte

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

#pragma once

#include <Ecs.hh>
#include <algorithm>
#include <functional>

DAE_NAMESPACE_BEGIN

	/* Base call class has a single virtual execute, used by CallList template below */
	template<typename ..._Args>
	class ICall {
	public:
		virtual ~ICall() = default;
		virtual void execute(_Args...) = 0;
	};

	/* Object + method ptr call implementation */
	template<typename _Object, typename ..._Args>
	class ObjectCall : public ICall<_Args...> {
	public:
		using ObjectType = _Object;
		using MethodPtr = void(ObjectType::*)(_Args...);
		ObjectCall() = default;
		ObjectCall(ObjectType* object, MethodPtr method) 
			: m_object(object)
			, m_method(method)
		{
		}
		void execute(_Args... args) override {
			if (m_object && m_method)
				(m_object->*m_method)(std::forward<_Args>(args)...);
		}
	private:
		ObjectType*	m_object = nullptr;
		MethodPtr	m_method = nullptr;
	};

	/* Entity + component + method ptr call implementation */
	template<typename _Component, typename ..._Args>
	class EntityCall : public ICall<_Args...> {
	public:
		using ComponentType = _Component;
		using MethodPtr = void(ComponentType::*)(_Args...);
		EntityCall() = default;
		EntityCall(Entity entity, MethodPtr method)
			: m_entity(entity)
			, m_method(method)
		{}
		void execute(_Args... args) override {
			if (m_method) {
				if (auto comp = m_entity.get<ComponentType>())
					(comp->*m_method)(std::forward<_Args>(args)...);
				else
					m_entity = {};		// if it failed once, make sure entity goes to empty to prevent calling into invalid entity
			}
		}
	private:
		Entity		m_entity;
		MethodPtr	m_method = nullptr;
	};

	/* std::function call implementation */
	template<typename ..._Args>
	class StdFunctionCall : public ICall<_Args...> {
	public:
		using FunctionType = std::function<void(_Args...)>;
		StdFunctionCall() = default;
		StdFunctionCall(FunctionType function)
			: m_function(std::move(function))
		{}
		void execute(_Args... args) override {
			if (m_function)
				m_function(std::forward<_Args>(args)...);
		}
	private:
		FunctionType	m_function;
	};

	/* Call list that can take any kind of call implementation.
	 * Can be used with any kind of "smart" pointer, unique, shared, or custom. 
	 */
	template<template<typename...> class _PtrType, typename ..._Args>
	class CallList {
	public:
		using IEntityCallType = ICall<_Args...>;
		using PtrType = _PtrType<IEntityCallType>;

		PtrType add(PtrType type) {
			return m_list.emplace_back(std::move(type));
		}
		template<typename _Component>
		PtrType add(Entity entity, void(_Component::*method)(_Args...)) {
			return m_list.emplace_back(PtrType{ new EntityCall(entity, method) });
		}
		template<typename _ObjectType>
		PtrType add(_ObjectType* object, void(_ObjectType::*method)(_Args...)) {
			return m_list.emplace_back(PtrType{ new ObjectCall(object, method) });
		}
		PtrType add(std::function<void(_Args...)> function) {
			return m_list.emplace_back(PtrType{ new StdFunctionCall(std::move(function)) });
		}
		void remove(PtrType type) {
			m_list.erase(std::remove(m_list.begin(), m_list.end(), type),
						 m_list.end());
		}
		template<typename ..._ExecArgs>
		void execute(_ExecArgs&&... args) {
			for (const auto& ptr : m_list)
				ptr->execute(std::forward<_ExecArgs>(args)...);
		}

	private:
		std::vector<PtrType>		m_list;
	};

DAE_NAMESPACE_END
