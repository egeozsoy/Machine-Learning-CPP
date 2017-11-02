// NeuralNet1.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include <cmath>
#include <iostream>
#include <vector>
#include <random>
#include "boost\numeric\ublas\vector.hpp"
#include "boost\numeric\ublas\matrix.hpp"
#include "mnist_loader.h"

using namespace boost::numeric;

std::random_device rd;
std::mt19937 gen(rd());

void Randomize(ublas::vector<double> &vec)
{
	std::normal_distribution<> d(0, 1);
	for (auto &e : vec) { e = d(gen); }
}

void Randomize(ublas::matrix<double> &m)
{
	std::normal_distribution<> d(0, 1);
	for (auto &e : m.data()) { e = d(gen); }
}

double sigmoid(double z)
{
	return 1.0 / (1.0 + exp(-z));
}

void sigmoid(ublas::vector<double> &v)
{
	for (auto &e : v) { e = sigmoid(e); }
}

double sigmoid_prime(double z)
{
	auto zp = sigmoid(z);
	return zp*(1 - zp);
}

void sigmoid_prime(ublas::vector<double> &v)
{
	for (auto &e : v) { e = sigmoid_prime(e); }
}

class Network {
private:
	using BiasesVector = std::vector<ublas::vector<double>>;
	using WeightsVector = std::vector<ublas::matrix<double>>;
	std::vector<int> m_sizes;
	BiasesVector biases;
	WeightsVector weights;
public:
	Network(std::vector<int> sizes)
		: m_sizes(sizes)
	{
		PopulateZeroWeightsAndBiases(biases, weights);
		for (auto &b : biases) Randomize(b);
		for (auto &w : weights) Randomize(w);
	}

	void PopulateZeroWeightsAndBiases(BiasesVector &b, WeightsVector &w)  const
	{
		for (size_t i = 1; i < m_sizes.size(); ++i)
		{
			b.push_back(ublas::zero_vector<double>(m_sizes[i]));
			w.push_back(ublas::zero_matrix<double>(m_sizes[i], m_sizes[i - 1]));
		}
	}

	ublas::vector<double> feedforward(ublas::vector<double> a) const
	{
		for (size_t i = 0; i < biases.size(); ++i)
		{
			ublas::vector<double> c = prod(weights[i], a) + biases[i];
			sigmoid(c);
			a = c;
		}
		return a;
	}

	using TrainingData = std::pair<ublas::vector<double>, ublas::vector<double>>;

	void SGD(std::vector<TrainingData> training_data, int epochs, int mini_batch_size, double eta,
		std::vector<TrainingData> test_data)
	{
		for (int j = 0; j < epochs; j++)
		{
			std::random_shuffle(training_data.begin(), training_data.end());
			for (size_t i = 0; i < training_data.size(); i += mini_batch_size) {
				auto iter = training_data.begin();
				std::advance(iter, i);
				update_mini_batch(iter, mini_batch_size, eta);
			}
			if (test_data.size() != 0)
				std::cout << "Epoch " << j << ": " << evaluate(test_data) << " / " << test_data.size() << std::endl;
			else
				std::cout << "Epoch " << j << " complete" << std::endl;

		}
	}
	/// Update the network's weights and biases by applying
	///	gradient descent using backpropagation to a single mini batch.
	///	The "mini_batch" is a list of tuples "(x, y)", and "eta"
	///	is the learning rate."""
	void update_mini_batch(std::vector<TrainingData>::iterator td, int mini_batch_size, double eta)
	{
		std::vector<ublas::vector<double>> nabla_b;
		std::vector<ublas::matrix<double>> nabla_w;
		PopulateZeroWeightsAndBiases(nabla_b, nabla_w);
		for (int i = 0; i < mini_batch_size; ++i, td++) {
			ublas::vector<double> x = td->first; // test data
			ublas::vector<double> y = td->second; // expected result
			std::vector<ublas::vector<double>> delta_nabla_b;
			std::vector<ublas::matrix<double>> delta_nabla_w;
			PopulateZeroWeightsAndBiases(delta_nabla_b, delta_nabla_w);
			backprop(x, y, delta_nabla_b, delta_nabla_w);
			for (size_t k = 0; k < biases.size(); ++k)
			{
				nabla_b[k] += delta_nabla_b[k];
				nabla_w[k] += delta_nabla_w[k];
			}
		}
		for (size_t i = 0; i < biases.size(); ++i)
		{
			biases[i] -= eta / mini_batch_size * nabla_b[i];
			weights[i] -= eta / mini_batch_size * nabla_w[i];
		}
	}

	void backprop(const ublas::vector<double> &x, const ublas::vector<double> &y,
		std::vector<ublas::vector<double>> &nabla_b,
		std::vector<ublas::matrix<double>> &nabla_w)
	{
		auto activation = x;
		std::vector<ublas::vector<double>> activations;
		activations.push_back(x);
		std::vector<ublas::vector<double>> zs;
		for (size_t i = 0; i < biases.size(); ++i) {
			ublas::vector<double> z = prod(weights[i], activation) + biases[i];
			zs.push_back(z);
			activation = z;
			sigmoid(activation);
			activations.push_back(activation);
		}
		// backward pass
		auto iActivations = activations.end() - 1;
		auto izs = zs.end() - 1;
		sigmoid_prime(*izs);
		ublas::vector<double> delta = element_prod(cost_derivative(*iActivations, y), *izs);
		auto ib = nabla_b.end() - 1;
		auto iw = nabla_w.end() - 1;
		*ib = delta;
		iActivations--;
		*iw = outer_prod(delta, trans(*iActivations));
		auto iWeights = weights.end();
		while (iActivations != activations.begin())
		{
			izs--; iWeights--; iActivations--; ib--; iw--;
			sigmoid_prime(*izs);
			delta = element_prod(prod(trans(*iWeights), delta), *izs);
			*ib = delta;
			*iw = outer_prod(delta, trans(*iActivations));
		}
	}

	int evaluate(const std::vector<TrainingData> &td) const
	{
		return count_if(td.begin(), td.end(), [this](const TrainingData &testElement) {
			auto res = feedforward(testElement.first);
			return (std::distance(res.begin(), max_element(res.begin(), res.end()))
				== std::distance(testElement.second.begin(), max_element(testElement.second.begin(), testElement.second.end()))
				);
		});
	}

	ublas::vector<double> cost_derivative(const ublas::vector<double>& output_activations,
		const ublas::vector<double>& y) const
	{
		return output_activations - y;
	}
};

int main()
{

	std::vector<Network::TrainingData> td, testData;
	mnist_loader<double> loader("C:\\Users\\Gareth\\Source\\Repos\\NeuralNet1\\Data\\train-images.idx3-ubyte",
		"C:\\Users\\Gareth\\Source\\Repos\\NeuralNet1\\Data\\train-labels.idx1-ubyte",
		td);
	mnist_loader<double> loader2("C:\\Users\\Gareth\\Source\\Repos\\NeuralNet1\\Data\\t10k-images.idx3-ubyte",
		"C:\\Users\\Gareth\\Source\\Repos\\NeuralNet1\\Data\\t10k-labels.idx1-ubyte",
		testData);

	Network net({ 784, 30, 10 });
	net.SGD(td, 30, 10, 3.0, testData);

	return 0;
}
