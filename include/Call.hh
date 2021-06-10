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

DAE_NAMESPACE_BEGIN

	template<typename _Return, typename ..._Args>
	class ICall {
	public:
		virtual ~ICall() = default;
		virtual _Return execute(_Args...) = 0;
	};

	/* Generic ObjectCall */
	template<typename _Return, typename _Object, typename ..._Args>
	class ObjectCall : public ICall<_Return, _Args...> {
	public:
		using ReturnType = _Return;
		using ObjectType = _Object;
		using MethodPtr = ReturnType(ObjectType::*)(_Args...);
		ObjectCall() = default;
		ObjectCall(ObjectType* object, MethodPtr method) 
			: m_object(object)
			, m_method(method)
		{
		}
		ReturnType execute(_Args... args) override {
			if (m_object && m_method)
				return (m_object->*m_method)(std::forward<_Args>(args)...);
			return {};
		}
	private:
		ObjectType*	m_object = nullptr;
		MethodPtr	m_method = nullptr;
	};

	/* Void return type overload of ObjectCall */
	template<typename _Object, typename ..._Args>
	class ObjectCall<void, _Object, _Args...> : public ICall<void, _Args...> {
	public:
		using ObjectType = _Object;
		using MethodPtr = void(ObjectType::*)(_Args...);
		ObjectCall() = default;
		ObjectCall(ObjectType* object, MethodPtr method)
			: m_object(object)
			, m_method(method) {}
		void execute(_Args... args) override {
			if (m_object && m_method)
				(m_object->*m_method)(std::forward<_Args>(args)...);
		}
	private:
		ObjectType* m_object = nullptr;
		MethodPtr	m_method = nullptr;
	};

	/* Generic EntityCall */
	template<typename _Return, typename _Component, typename ..._Args>
	class EntityCall : public ICall<_Return, _Args...> {
	public:
		using ReturnType = _Return;
		using ComponentType = _Component;
		using MethodPtr = ReturnType(ComponentType::*)(_Args...);
		EntityCall() = default;
		EntityCall(Entity entity, MethodPtr method)
			: m_entity(entity)
			, m_method(method)
		{}
		ReturnType execute(_Args... args) override {
			if (m_method) {
				if (auto comp = m_entity.get<ComponentType>())
					return (comp->*m_method)(std::forward<_Args>(args)...);
				else
					m_entity = {};		// if it failed once, make sure entity goes to empty to prevent calling into invalid entity
			}
			return ReturnType{};
		}
	private:
		Entity		m_entity;
		MethodPtr	m_method = nullptr;
	};

	/* Void return type overload of EntityCall */
	template<typename _Component, typename ..._Args>
	class EntityCall<void, _Component, _Args...> : public ICall<void, _Args...> {
	public:
		using ReturnType = void;
		using ComponentType = _Component;
		using MethodPtr = void(ComponentType::*)(_Args...);
		EntityCall() = default;
		EntityCall(Entity entity, MethodPtr method)
			: m_entity(entity)
			, m_method(method) {}
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

	/* Call list that can take any kind of call implementation.
	 * Can be used with any kind of "smart" pointer, unique, shared, or custom. 
	 */
	template<template<typename...> class _PtrType, typename ..._Args>
	class CallList {
	public:
		using IEntityCallType = ICall<void, _Args...>;
		using PtrType = _PtrType<IEntityCallType>;

		void add(PtrType type) {
			m_list.emplace_back(std::move(type));
		}
		template<typename _Component>
		void add(Entity entity, void(_Component::*method)(_Args...)) {
			m_list.emplace_back(PtrType{ new EntityCall(entity, method) });
		}
		template<typename _ObjectType>
		void add(_ObjectType* object, void(_ObjectType::*method)(_Args...)) {
			m_list.emplace_back(PtrType{ new ObjectCall(object, method) });
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
