#pragma once

#include "Cards/CardDispatcher.h"

#include "state/State.h"
#include "FlowControl/FlowController.h"
#include "FlowControl/FlowController-impl.h"
#include "Cards/Database.h"
#include "Cards/Cards.h"

namespace Cards
{
	namespace detail
	{
		template <typename T> class ConstructorInvoker
		{
		public:
			static state::Cards::CardData Invoke()
			{
				return T();
			}
		};

#define CREATE_INVOKER(METHOD_NAME) \
			template <typename T> class METHOD_NAME ## Invoker \
			{ \
			private: \
				typedef long HasMethodType; \
				typedef char NoMethodType; \
				template <typename U, typename V = decltype(&T::METHOD_NAME)> struct SFINAE { using type = HasMethodType; }; \
				template <typename U> static typename SFINAE<T>::type MethodExists(void*); \
				template <typename U> static NoMethodType MethodExists(...); \
\
			public: \
				template <typename... Args> static void Invoke(Args&&... args) { \
					using TestResult = decltype(MethodExists<T>(nullptr)); \
					return InvokeInternal<TestResult>(nullptr, std::forward<Args>(args)...); \
				} \
\
			private: \
				template <typename TestResult, typename... Args> \
				static void InvokeInternal(std::enable_if_t<sizeof(TestResult) == sizeof(HasMethodType)>*, Args&&... args) \
				{ \
					return T::METHOD_NAME(std::forward<Args>(args)...); \
				} \
\
				template <typename TestResult, typename... Args> \
				static void InvokeInternal(std::enable_if_t<sizeof(TestResult) == sizeof(NoMethodType)>*, Args&&... args) \
				{ \
					return; \
				} \
			};

#undef CREATE_INVOKER

		template <int id>
		class DefaultInvoked : public state::Cards::CardData
		{
		public:
			DefaultInvoked()
			{
				// TODO: should unify with Cards/framework/xxxxCardBase.h
				this->card_id = (Cards::CardId)id;
				auto const& data = Cards::Database::GetInstance().Get(this->card_id);

				this->card_type = data.card_type;
				this->card_race = data.card_race;
				this->card_rarity = data.card_rarity;

				this->enchanted_states.cost = data.cost;
				this->enchanted_states.attack = data.attack;
				this->enchanted_states.max_hp = data.max_hp;

				switch (data.card_type) {
				case state::kCardTypeMinion:
				case state::kCardTypeWeapon:
				case state::kCardTypeHero:
					return; // allow default invoke
				default:
					throw std::exception("card is not implemented");
				}
			}
		};
	}

	state::Cards::CardData CardDispatcher::CreateInstance(CardId id)
	{
		return DispatcherImpl::Invoke<detail::ConstructorInvoker, state::Cards::CardData>((int)id);
	}
}