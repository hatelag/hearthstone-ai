#pragma once

#include "..\GameEngineCppWrapper\Header.h"

namespace GameEngineCppWrapper
{
	namespace CLI
	{
		public ref class GameEngine
		{
		public:
			GameEngine();
			~GameEngine();
			!GameEngine();

			static void InitializeLibrary(System::String^ path);

			int Get();

			void Destroy();

		private:
			::GameEngineCppWrapper::GameEngine* impl_;
		};
	}
}