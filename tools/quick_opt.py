from rss_super_pipeline import RSSSuperPipeline

if __name__ == '__main__':
    pipeline = RSSSuperPipeline(n_trials=50, n_jobs=1)
    result = pipeline.optimize()
    print('Finished quick test, best trials count', result['n_solutions'])
