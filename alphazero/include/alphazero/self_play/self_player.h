#pragma once

#include <vector>
#include <memory>
#include <sstream>

#include "alphazero/neural_net/neural_net.h"
#include "alphazero/self_play/self_player_options.h"
#include "alphazero/shared_data/training_data.h"
#include "alphazero/logger.h"

namespace alphazero
{
	namespace self_play
	{
		class SelfPlayer
		{
		public:
			SelfPlayer(ILogger & logger) : logger_(logger), items_(), data_(nullptr) {}

			void BeforeRun(shared_data::TrainingData & data) {
				assert(items_.empty());
				data_ = &data;
				generated_data_ = 0;
			}

			// Thread safety: No
			void RunOnce(neural_net::NeuralNet const& neural_net, SelfPlayerRunOptions const& options) {
				// TODO: use MCTS to generate self play data
				// Choose actions proportions to its visit counts.
				// TODO: Add Dirchlet distribution to root node. This further ensures the variety.

				for (int i = 0; i < 100; ++i) {
					items_.push_back(std::make_shared<shared_data::TrainingDataItem>());
					++generated_data_;
				}

				data_->PushN(items_);
				assert(items_.empty());
			}

			void AfterRun() {
				assert(items_.empty());
				std::stringstream ss;
				ss << "Generated " << generated_data_ << " data.";
				logger_.Info(ss.str());
			}

		private:
			ILogger & logger_;
			std::vector<std::shared_ptr<shared_data::TrainingDataItem>> items_;
			shared_data::TrainingData * data_;

			size_t generated_data_;
		};
	}
}